"""Safe URL fetching for the Intake agent's sourceMaterial.url — NOT WIRED UP YET.

Nothing calls fetch_url_content() anywhere in this codebase. GenerationRequest.url is
accepted and stored, but Intake's build_user_prompt() currently receives url_content as
a plain parameter the caller must supply — no caller exists yet. This module exists so
that when one is written, it doesn't reach for requests.get(url) directly: fetching an
arbitrary user-supplied URL from the server is a textbook SSRF vector (cloud metadata
endpoints, internal services, localhost, other hosts on the same private network), and
that risk needs to be closed before this is ever wired in, not after.

Defenses here:
- http(s) only — no file:, ftp:, gopher:, etc.
- Resolve the hostname and reject any address that is private, loopback, link-local,
  reserved, or multicast (blocks 127.0.0.1, 169.254.169.254 cloud metadata, 10.x/172.16.x
  /192.168.x internal ranges, etc.) — checked for every hop of a redirect, not just the
  original URL, since a public URL can still redirect to an internal one.
- A short connect/read timeout, so one slow host can't tie up a request.
- A hard cap on response size, enforced while streaming (not after downloading
  everything) so a huge or infinite response can't exhaust memory.
- A small, bounded number of redirects, each independently re-validated.
"""

import ipaddress
import socket
from urllib.parse import urlparse

import httpx

ALLOWED_SCHEMES = {"http", "https"}
MAX_BYTES = 1_000_000  # 1 MB of page text is plenty of "reference material"
TIMEOUT_SECONDS = 5.0
MAX_REDIRECTS = 3
USER_AGENT = "ThreadBare/0.1 (+https://github.com/DrJohnWagner/ThreadBare)"


class UnsafeUrlError(ValueError):
    """Raised when a URL (or a redirect target) fails the SSRF checks."""


def _resolved_addresses_are_safe(hostname: str) -> bool:
    try:
        infos = socket.getaddrinfo(hostname, None)
    except socket.gaierror:
        return False
    for info in infos:
        addr = info[4][0]
        ip = ipaddress.ip_address(addr)
        if (
            ip.is_private
            or ip.is_loopback
            or ip.is_link_local
            or ip.is_reserved
            or ip.is_multicast
            or ip.is_unspecified
        ):
            return False
    return True


def _assert_url_is_safe(url: str) -> None:
    parsed = urlparse(url)
    if parsed.scheme not in ALLOWED_SCHEMES:
        raise UnsafeUrlError(f"unsupported URL scheme: {parsed.scheme!r}")
    if not parsed.hostname:
        raise UnsafeUrlError("URL has no hostname")
    if not _resolved_addresses_are_safe(parsed.hostname):
        raise UnsafeUrlError(
            f"refusing to fetch {url!r} — resolves to a private, loopback, or "
            "otherwise disallowed address"
        )


def fetch_url_content(url: str) -> str:
    """Fetch `url` and return its body as text, or raise UnsafeUrlError/httpx errors.

    Re-validates the target after every redirect hop, not just the original URL.
    """
    current_url = url
    with httpx.Client(
        timeout=TIMEOUT_SECONDS,
        follow_redirects=False,
        headers={"User-Agent": USER_AGENT},
    ) as client:
        for _ in range(MAX_REDIRECTS + 1):
            _assert_url_is_safe(current_url)
            response = client.get(current_url)
            if response.is_redirect:
                next_url = response.headers.get("location")
                if not next_url:
                    raise UnsafeUrlError("redirect response had no Location header")
                current_url = str(httpx.URL(current_url).join(next_url))
                continue

            response.raise_for_status()
            content_length = response.headers.get("content-length")
            if content_length is not None and int(content_length) > MAX_BYTES:
                raise UnsafeUrlError(
                    f"response too large ({content_length} bytes, "
                    f"limit {MAX_BYTES})"
                )

            body = b""
            for chunk in response.iter_bytes():
                body += chunk
                if len(body) > MAX_BYTES:
                    raise UnsafeUrlError(
                        f"response exceeded {MAX_BYTES} bytes while streaming"
                    )
            return body.decode(response.encoding or "utf-8", errors="replace")

    raise UnsafeUrlError(f"too many redirects (limit {MAX_REDIRECTS})")

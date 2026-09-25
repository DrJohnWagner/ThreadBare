const API_BASE = 'http://127.0.0.1:8000'

export class ApiError extends Error {
    status: number

    constructor(status: number, message: string) {
        super(message)
        this.status = status
    }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
    let response: Response
    try {
        response = await fetch(`${API_BASE}${path}`, {
            headers: { 'Content-Type': 'application/json' },
            ...init,
        })
    } catch {
        throw new ApiError(0, 'Could not reach the backend. Is it running?')
    }

    if (!response.ok) {
        throw new ApiError(
            response.status,
            `Request to ${path} failed (${response.status})`,
        )
    }

    if (response.status === 204) {
        return undefined as T
    }

    return (await response.json()) as T
}

export function apiGet<T>(path: string): Promise<T> {
    return request<T>(path)
}

export function apiPost<T>(path: string, body: unknown): Promise<T> {
    return request<T>(path, { method: 'POST', body: JSON.stringify(body) })
}

export function apiPatch<T>(path: string, body: unknown): Promise<T> {
    return request<T>(path, { method: 'PATCH', body: JSON.stringify(body) })
}

export function apiDelete(path: string): Promise<void> {
    return request<void>(path, { method: 'DELETE' })
}

export function apiUrl(path: string): string {
    return `${API_BASE}${path}`
}

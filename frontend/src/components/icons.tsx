export function CodeIcon() {
    return (
        <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2.2"
            strokeLinecap="round"
            strokeLinejoin="round"
        >
            <polyline points="8 6 2 12 8 18" />
            <polyline points="16 6 22 12 16 18" />
        </svg>
    )
}

export function TextIcon() {
    return (
        <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2.2"
            strokeLinecap="round"
            strokeLinejoin="round"
        >
            <line x1="4" y1="6" x2="20" y2="6" />
            <line x1="4" y1="12" x2="20" y2="12" />
            <line x1="4" y1="18" x2="14" y2="18" />
        </svg>
    )
}

export function LinkIcon() {
    return (
        <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2.2"
            strokeLinecap="round"
            strokeLinejoin="round"
        >
            <path d="M9 15l6-6" />
            <path d="M10.5 6.5l1-1a3.5 3.5 0 015 5l-1 1" />
            <path d="M13.5 17.5l-1 1a3.5 3.5 0 01-5-5l1-1" />
        </svg>
    )
}

interface ChevronIconProps {
    open: boolean
}

export function ChevronIcon({ open }: ChevronIconProps) {
    return (
        <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2.4"
            strokeLinecap="round"
            strokeLinejoin="round"
            style={{
                transform: open ? 'rotate(180deg)' : 'none',
                transition: 'transform 150ms',
            }}
        >
            <polyline points="6 9 12 15 18 9" />
        </svg>
    )
}

export function Logo() {
    return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
            <circle
                cx="5"
                cy="12"
                r="2.4"
                style={{ fill: 'var(--color-performance)' }}
            />
            <circle
                cx="19"
                cy="5"
                r="2.4"
                style={{ fill: 'var(--color-safety)' }}
            />
            <circle
                cx="19"
                cy="19"
                r="2.4"
                style={{ fill: 'var(--color-liveness)' }}
            />
            <path
                d="M7 12 L17 5.5 M7 12 L17 18.5"
                style={{
                    stroke: 'var(--color-border-strong)',
                    strokeWidth: 1.4,
                }}
            />
        </svg>
    )
}

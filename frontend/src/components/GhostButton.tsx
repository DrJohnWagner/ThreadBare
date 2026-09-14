import type { ReactNode } from 'react'
import './ui.css'

interface GhostButtonProps {
    children: ReactNode
    onClick?: () => void
    disabled?: boolean
}

export function GhostButton({ children, onClick, disabled }: GhostButtonProps) {
    return (
        <button
            type="button"
            className="btn btn-ghost"
            onClick={onClick}
            disabled={disabled}
        >
            {children}
        </button>
    )
}

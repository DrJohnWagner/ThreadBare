import type { ReactNode } from 'react'
import './ui.css'

interface PrimaryButtonProps {
    children: ReactNode
    onClick?: () => void
    disabled?: boolean
}

export function PrimaryButton({
    children,
    onClick,
    disabled,
}: PrimaryButtonProps) {
    return (
        <button
            type="button"
            className="btn btn-primary"
            onClick={onClick}
            disabled={disabled}
        >
            {children}
        </button>
    )
}

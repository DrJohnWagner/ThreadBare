import type { ReactNode } from 'react'
import './ui.css'

interface TagProps {
    children: ReactNode
    color?: string
}

export function Tag({ children, color }: TagProps) {
    return (
        <span className="tag" style={color ? { color } : undefined}>
            {children}
        </span>
    )
}

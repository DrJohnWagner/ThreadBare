import './ui.css'

interface NumberBadgeProps {
    n: number
}

export function NumberBadge({ n }: NumberBadgeProps) {
    return <span className="number-badge">{n}</span>
}

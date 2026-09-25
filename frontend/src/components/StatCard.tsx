import './DashboardCards.css'

interface StatCardProps {
    value: string | number
    label: string
}

export function StatCard({ value, label }: StatCardProps) {
    return (
        <div className="stat-card">
            <div className="stat-card__value">{value}</div>
            <div className="stat-card__label">{label}</div>
        </div>
    )
}

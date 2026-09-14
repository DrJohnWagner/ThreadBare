import './DashboardCards.css'

export interface BreakdownEntry {
    key: string
    label: string
    count: number
    color?: string
}

interface BreakdownCardProps {
    title: string
    entries: BreakdownEntry[]
}

export function BreakdownCard({ title, entries }: BreakdownCardProps) {
    return (
        <div className="breakdown-card">
            <h3>{title}</h3>
            {entries.length === 0 ? (
                <p className="breakdown-card__empty">No data yet.</p>
            ) : (
                <ul>
                    {entries.map((entry) => (
                        <li key={entry.key}>
                            <span className="breakdown-card__entry-label">
                                {entry.color && (
                                    <span
                                        className="breakdown-card__dot"
                                        style={{ backgroundColor: entry.color }}
                                    />
                                )}
                                {entry.label}
                            </span>
                            <span className="breakdown-card__count">
                                {entry.count}
                            </span>
                        </li>
                    ))}
                </ul>
            )}
        </div>
    )
}

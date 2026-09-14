import { useDashboardStats } from '../hooks/useDashboardStats'
import { StatCard } from '../components/StatCard'
import { BreakdownCard } from '../components/BreakdownCard'
import type { Taxonomy } from '../types/taxonomy'
import {
    colorForCategory,
    labelForCategory,
    labelForType,
} from '../lib/taxonomyLookup'
import type { FailureCategory } from '../types/taxonomy'
import './DashboardPage.css'

interface DashboardPageProps {
    taxonomy: Taxonomy
    refreshToken: number
}

export function DashboardPage({ taxonomy, refreshToken }: DashboardPageProps) {
    const { stats, loading, error } = useDashboardStats(refreshToken)

    return (
        <div className="dashboard-page">
            <h1 className="dashboard-page__title">Dashboard</h1>
            <p className="dashboard-page__subtitle">
                Aggregate stats from every run this backend has generated.
            </p>

            {loading && <p>Loading…</p>}
            {error && <div className="dashboard-page__notice">{error}</div>}

            {stats && stats.totalRuns === 0 && (
                <div className="dashboard-page__notice">
                    No runs yet. Generate one in the Lab to see it here.
                </div>
            )}

            {stats && stats.totalRuns > 0 && (
                <>
                    <div className="dashboard-page__stats">
                        <StatCard
                            value={stats.totalRuns}
                            label="Runs generated"
                        />
                        <StatCard
                            value={stats.fixedCount}
                            label="Fix Bugs requested"
                        />
                        <StatCard
                            value={
                                stats.fixRate === null
                                    ? '—'
                                    : `${Math.round(stats.fixRate * 100)}%`
                            }
                            label="Fix request rate"
                        />
                    </div>
                    <div className="dashboard-page__breakdowns">
                        <BreakdownCard
                            title="By category"
                            entries={(
                                Object.entries(stats.byCategory) as [
                                    FailureCategory,
                                    number,
                                ][]
                            )
                                .filter(([, count]) => count > 0)
                                .map(([key, count]) => ({
                                    key,
                                    label: labelForCategory(taxonomy, key),
                                    count,
                                    color: colorForCategory(key),
                                }))}
                        />
                        <BreakdownCard
                            title="By failure type"
                            entries={Object.entries(stats.byType).map(
                                ([key, count]) => ({
                                    key,
                                    label: labelForType(taxonomy, key),
                                    count,
                                }),
                            )}
                        />
                    </div>
                </>
            )}
        </div>
    )
}

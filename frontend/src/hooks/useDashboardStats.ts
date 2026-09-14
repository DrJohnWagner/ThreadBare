import { useEffect, useState } from 'react'
import { getDashboardStats } from '../api/dashboard'
import type { DashboardStats } from '../types/run'

interface UseDashboardStatsResult {
    stats: DashboardStats | null
    loading: boolean
    error: string | null
}

export function useDashboardStats(
    refreshToken: number,
): UseDashboardStatsResult {
    const [stats, setStats] = useState<DashboardStats | null>(null)
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    useEffect(() => {
        let cancelled = false
        getDashboardStats()
            .then((data) => {
                if (cancelled) return
                setStats(data)
                setError(null)
            })
            .catch(() => {
                if (!cancelled) setError('Could not load dashboard data.')
            })
            .finally(() => {
                if (!cancelled) setLoading(false)
            })
        return () => {
            cancelled = true
        }
    }, [refreshToken])

    return { stats, loading, error }
}

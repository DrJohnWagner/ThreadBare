import { useCallback, useEffect, useState } from 'react'
import { clearRuns, listRuns } from '../api/runs'
import type { HistoryRecord } from '../types/run'

interface UseHistoryResult {
    records: HistoryRecord[] | null
    loading: boolean
    error: string | null
    refresh: () => void
    clear: () => Promise<void>
}

export function useHistory(refreshToken: number): UseHistoryResult {
    const [records, setRecords] = useState<HistoryRecord[] | null>(null)
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)
    const [localToken, setLocalToken] = useState(0)

    const load = useCallback(() => {
        let cancelled = false
        listRuns()
            .then((data) => {
                if (cancelled) return
                setRecords(data)
                setError(null)
            })
            .catch(() => {
                if (!cancelled) setError('Could not load history.')
            })
            .finally(() => {
                if (!cancelled) setLoading(false)
            })
        return () => {
            cancelled = true
        }
    }, [])

    useEffect(() => load(), [load, refreshToken, localToken])

    const refresh = () => setLocalToken((n) => n + 1)

    const clear = async () => {
        await clearRuns()
        refresh()
    }

    return { records, loading, error, refresh, clear }
}

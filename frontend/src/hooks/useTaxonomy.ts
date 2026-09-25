import { useEffect, useState } from 'react'
import { getTaxonomy } from '../api/taxonomy'
import type { Taxonomy } from '../types/taxonomy'

interface UseTaxonomyResult {
    taxonomy: Taxonomy | null
    loading: boolean
    error: string | null
}

export function useTaxonomy(): UseTaxonomyResult {
    const [taxonomy, setTaxonomy] = useState<Taxonomy | null>(null)
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState<string | null>(null)

    useEffect(() => {
        let cancelled = false
        getTaxonomy()
            .then((data) => {
                if (!cancelled) setTaxonomy(data)
            })
            .catch(() => {
                if (!cancelled)
                    setError('Could not load the failure-mode taxonomy.')
            })
            .finally(() => {
                if (!cancelled) setLoading(false)
            })
        return () => {
            cancelled = true
        }
    }, [])

    return { taxonomy, loading, error }
}

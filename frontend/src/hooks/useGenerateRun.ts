import { useState } from 'react'
import { generateRun as generateRunRequest } from '../api/runs'
import { ApiError } from '../api/client'
import type { GenerationRequest, Run } from '../types/run'

interface UseGenerateRunResult {
    run: Run | null
    loading: boolean
    error: string | null
    generate: (body: GenerationRequest) => Promise<void>
    setRun: (run: Run) => void
}

export function useGenerateRun(): UseGenerateRunResult {
    const [run, setRun] = useState<Run | null>(null)
    const [loading, setLoading] = useState(false)
    const [error, setError] = useState<string | null>(null)

    const generate = async (body: GenerationRequest) => {
        setLoading(true)
        setError(null)
        try {
            const result = await generateRunRequest(body)
            setRun(result)
        } catch (err) {
            setError(
                err instanceof ApiError
                    ? err.message
                    : 'Something went wrong generating that run.',
            )
        } finally {
            setLoading(false)
        }
    }

    return { run, loading, error, generate, setRun }
}

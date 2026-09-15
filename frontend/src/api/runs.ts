import { apiDelete, apiGet, apiPost } from './client'
import type { GenerationRequest, HistoryRecord, Run } from '../types/run'

export function generateRun(body: GenerationRequest): Promise<Run> {
    return apiPost<Run>('/api/runs', body)
}

export function listRuns(): Promise<HistoryRecord[]> {
    return apiGet<HistoryRecord[]>('/api/runs')
}

export function getRun(id: string): Promise<Run> {
    return apiGet<Run>(`/api/runs/${id}`)
}

export function clearRuns(): Promise<void> {
    return apiDelete('/api/runs')
}

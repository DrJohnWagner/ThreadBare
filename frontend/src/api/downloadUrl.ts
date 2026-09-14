import { apiUrl } from './client'

export function runDownloadUrl(id: string): string {
    return apiUrl(`/api/runs/${id}/download`)
}

export function historyExportUrl(): string {
    return apiUrl('/api/runs/export')
}

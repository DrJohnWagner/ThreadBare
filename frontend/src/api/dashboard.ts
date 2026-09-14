import { apiGet } from './client'
import type { DashboardStats } from '../types/run'

export function getDashboardStats(): Promise<DashboardStats> {
    return apiGet<DashboardStats>('/api/dashboard')
}

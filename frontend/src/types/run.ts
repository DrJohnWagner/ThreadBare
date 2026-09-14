// Mirrors schemas/generation-request.schema.json, planted-bug.schema.json,
// report-finding.schema.json, run.schema.json, history-record.schema.json,
// dashboard-stats.schema.json — see schemas/README.md.

export const LANGUAGE = 'c-openmp' as const
export type Language = typeof LANGUAGE

export interface SourceMaterial {
    code?: string
    text?: string
    url?: string
}

export interface GenerationRequest {
    language: Language
    failureModes: string[]
    sourceMaterial: SourceMaterial | null
}

export interface PlantedBug {
    typeKey: string
    implementationNote: string
}

export interface ReportFinding {
    typeKey: string
    lines: number[]
    explanation: string
}

export interface Run {
    id: string
    createdAt: string
    request: GenerationRequest
    serialReference: string
    parallelVersion: string
    parallelVersionFixed: string
    testHarness: string
    plantedBugs: PlantedBug[]
    report: ReportFinding[]
    fixed: boolean
}

export interface HistoryRecord {
    id: string
    createdAt: string
    language: Language
    requestedFailureModes: string[]
    plantedTypeKeys: string[]
    fixed: boolean
}

export interface CategoryCounts {
    safety: number
    performance: number
    liveness: number
}

export interface DashboardStats {
    totalRuns: number
    fixedCount: number
    fixRate: number | null
    byCategory: CategoryCounts
    byType: Record<string, number>
}

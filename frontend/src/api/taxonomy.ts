import { apiGet } from './client'
import type { Taxonomy } from '../types/taxonomy'

export function getTaxonomy(): Promise<Taxonomy> {
    return apiGet<Taxonomy>('/api/taxonomy')
}

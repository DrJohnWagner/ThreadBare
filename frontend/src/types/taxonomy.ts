// Mirrors schemas/taxonomy.schema.json — see schemas/README.md.

export type FailureCategory = 'safety' | 'performance' | 'liveness'

export interface TaxonomyItem {
    key: string
    label: string
    category: FailureCategory
    summary: string
    description: string
}

export interface TaxonomyCategory {
    key: FailureCategory
    label: string
    blurb: string
    items: TaxonomyItem[]
}

export interface Taxonomy {
    categories: TaxonomyCategory[]
}

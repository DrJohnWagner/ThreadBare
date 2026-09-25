import type { FailureCategory, Taxonomy } from '../types/taxonomy'

export function labelForType(taxonomy: Taxonomy, key: string): string {
    for (const category of taxonomy.categories) {
        const item = category.items.find((i) => i.key === key)
        if (item) return item.label
    }
    return key
}

export function categoryForType(
    taxonomy: Taxonomy,
    key: string,
): FailureCategory | null {
    for (const category of taxonomy.categories) {
        if (category.items.some((i) => i.key === key)) return category.key
    }
    return null
}

export function labelForCategory(
    taxonomy: Taxonomy,
    key: FailureCategory,
): string {
    return (
        taxonomy.categories.find((category) => category.key === key)?.label ??
        key
    )
}

export function colorForCategory(category: FailureCategory): string {
    return `var(--color-${category})`
}

export function softColorForCategory(category: FailureCategory): string {
    return `var(--color-${category}-soft)`
}

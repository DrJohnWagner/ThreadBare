import type { Taxonomy } from '../types/taxonomy'
import { FailureItem } from './FailureItem'
import { Tabs, type Tab } from './Tabs'
import './FailureCategorySelector.css'

interface FailureCategorySelectorProps {
    taxonomy: Taxonomy
    selectedTypes: string[]
    onToggle: (key: string) => void
    expandedKeys: Set<string>
    onToggleExpand: (key: string) => void
}

export function FailureCategorySelector({
    taxonomy,
    selectedTypes,
    onToggle,
    expandedKeys,
    onToggleExpand,
}: FailureCategorySelectorProps) {
    const tabs: Tab[] = taxonomy.categories.map((category) => {
        const selectedCount = category.items.filter((item) =>
            selectedTypes.includes(item.key),
        ).length

        return {
            key: category.key,
            label: (
                <span className="failure-category-tab-label">
                    <span
                        className="failure-category-tab-label__dot"
                        style={{
                            backgroundColor: `var(--color-${category.key})`,
                        }}
                    />
                    {category.label}
                    {selectedCount > 0 && (
                        <span className="failure-category-tab-label__count">
                            {selectedCount}
                        </span>
                    )}
                </span>
            ),
            content: (
                <div>
                    <p className="failure-category-panel__blurb">
                        {category.blurb}
                    </p>
                    <div className="failure-category-panel__items">
                        {category.items.map((item, i) => (
                            <FailureItem
                                key={item.key}
                                item={item}
                                checked={selectedTypes.includes(item.key)}
                                onToggle={() => onToggle(item.key)}
                                expanded={expandedKeys.has(item.key)}
                                onToggleExpand={() => onToggleExpand(item.key)}
                                isFirst={i === 0}
                            />
                        ))}
                    </div>
                </div>
            ),
        }
    })

    return <Tabs tabs={tabs} />
}

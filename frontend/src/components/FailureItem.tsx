import type { TaxonomyItem } from '../types/taxonomy'
import { colorForCategory, softColorForCategory } from '../lib/taxonomyLookup'
import { ChevronIcon } from './icons'
import './FailureCategorySelector.css'

interface FailureItemProps {
    item: TaxonomyItem
    checked: boolean
    onToggle: () => void
    expanded: boolean
    onToggleExpand: () => void
    isFirst: boolean
}

export function FailureItem({
    item,
    checked,
    onToggle,
    expanded,
    onToggleExpand,
    isFirst,
}: FailureItemProps) {
    const color = colorForCategory(item.category)
    return (
        <div
            className="failure-item"
            style={{
                backgroundColor: checked
                    ? softColorForCategory(item.category)
                    : 'var(--color-surface)',
                borderLeft: `4px solid ${color}`,
                borderTop: isFirst ? 'none' : '1px solid var(--color-border)',
            }}
        >
            <label className="failure-item__row">
                <input
                    type="checkbox"
                    checked={checked}
                    onChange={onToggle}
                    style={{ accentColor: color }}
                />
                <span className="failure-item__text">
                    <span className="failure-item__label">{item.label}</span>
                    <span className="failure-item__summary">
                        {item.summary}
                    </span>
                </span>
                <button
                    type="button"
                    className="failure-item__expand-toggle"
                    onClick={(event) => {
                        event.preventDefault()
                        onToggleExpand()
                    }}
                    aria-label="Show full description"
                >
                    <ChevronIcon open={expanded} />
                </button>
            </label>
            {expanded && (
                <div className="failure-item__description">
                    <p>{item.description}</p>
                </div>
            )}
        </div>
    )
}

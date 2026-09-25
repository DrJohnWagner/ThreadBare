import { useState } from 'react'
import type { HistoryRecord, Run } from '../types/run'
import type { Taxonomy } from '../types/taxonomy'
import { getRun } from '../api/runs'
import { runDownloadUrl } from '../api/downloadUrl'
import {
    categoryForType,
    colorForCategory,
    labelForType,
} from '../lib/taxonomyLookup'
import { ChevronIcon } from './icons'
import { Tag } from './Tag'
import { CodeBlock } from './CodeBlock'
import './HistoryRow.css'

interface HistoryRowProps {
    record: HistoryRecord
    taxonomy: Taxonomy
}

function categoryColorFor(taxonomy: Taxonomy, key: string): string | undefined {
    const category = categoryForType(taxonomy, key)
    return category ? colorForCategory(category) : undefined
}

export function HistoryRow({ record, taxonomy }: HistoryRowProps) {
    const [open, setOpen] = useState(false)
    const [run, setRun] = useState<Run | null>(null)
    const [loading, setLoading] = useState(false)

    const handleToggle = () => {
        setOpen((v) => !v)
        if (!run && !loading) {
            setLoading(true)
            getRun(record.id)
                .then(setRun)
                .finally(() => setLoading(false))
        }
    }

    return (
        <div className="history-row">
            <div
                className="history-row__header"
                role="button"
                tabIndex={0}
                onClick={handleToggle}
                onKeyDown={(event) => {
                    if (event.key === 'Enter' || event.key === ' ') {
                        event.preventDefault()
                        handleToggle()
                    }
                }}
            >
                <div>
                    <div className="history-row__tags">
                        {record.plantedTypeKeys.map((key) => (
                            <Tag
                                key={key}
                                color={categoryColorFor(taxonomy, key)}
                            >
                                {labelForType(taxonomy, key)}
                            </Tag>
                        ))}
                    </div>
                    <p className="history-row__timestamp">
                        {new Date(record.createdAt).toLocaleString()}
                    </p>
                </div>
                <div className="history-row__actions">
                    <a
                        href={runDownloadUrl(record.id)}
                        onClick={(event) => event.stopPropagation()}
                        className="history-row__download"
                    >
                        Download
                    </a>
                    <ChevronIcon open={open} />
                </div>
            </div>
            {open && (
                <div className="history-row__details">
                    {loading && <p>Loading…</p>}
                    {run && (
                        <>
                            <p className="history-row__label">
                                Parallel version
                            </p>
                            <CodeBlock code={run.parallelVersion} />
                            <p className="history-row__label">Fixed version</p>
                            <CodeBlock code={run.parallelVersionFixed} />
                        </>
                    )}
                </div>
            )}
        </div>
    )
}

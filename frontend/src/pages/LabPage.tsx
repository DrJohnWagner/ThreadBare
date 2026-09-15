import { useMemo, useState } from 'react'
import type { Taxonomy } from '../types/taxonomy'
import type { GenerationRequest } from '../types/run'
import { LANGUAGE } from '../types/run'
import { useGenerateRun } from '../hooks/useGenerateRun'
import { runDownloadUrl } from '../api/downloadUrl'
import { NumberBadge } from '../components/NumberBadge'
import { CodeIcon, LinkIcon, TextIcon, ChevronIcon } from '../components/icons'
import { FailureCategorySelector } from '../components/FailureCategorySelector'
import { JsonBlock } from '../components/JsonBlock'
import { PrimaryButton } from '../components/PrimaryButton'
import { Tabs, type Tab } from '../components/Tabs'
import { CodeBlock } from '../components/CodeBlock'
import { ReportPanel } from '../components/ReportPanel'
import './LabPage.css'

interface LabPageProps {
    taxonomy: Taxonomy
    onRunSaved: () => void
}

export function LabPage({ taxonomy, onRunSaved }: LabPageProps) {
    const [sourceCode, setSourceCode] = useState('')
    const [sourceText, setSourceText] = useState('')
    const [sourceUrl, setSourceUrl] = useState('')
    const [selectedTypes, setSelectedTypes] = useState<string[]>([])
    const [expandedKeys, setExpandedKeys] = useState<Set<string>>(new Set())
    const [showRequest, setShowRequest] = useState(false)

    const { run, loading, error: generateError, generate } = useGenerateRun()

    const allTypeKeys = useMemo(
        () =>
            taxonomy.categories.flatMap((category) =>
                category.items.map((item) => item.key),
            ),
        [taxonomy],
    )

    const toggleType = (key: string) =>
        setSelectedTypes((prev) =>
            prev.includes(key) ? prev.filter((k) => k !== key) : [...prev, key],
        )
    const toggleExpand = (key: string) =>
        setExpandedKeys((prev) => {
            const next = new Set(prev)
            if (next.has(key)) next.delete(key)
            else next.add(key)
            return next
        })
    const selectAll = () => setSelectedTypes(allTypeKeys)
    const deselectAll = () => setSelectedTypes([])

    const requestPayload: GenerationRequest = useMemo(() => {
        const hasSourceMaterial = sourceCode || sourceText || sourceUrl
        return {
            language: LANGUAGE,
            failureModes: selectedTypes,
            sourceMaterial: hasSourceMaterial
                ? {
                      code: sourceCode || undefined,
                      text: sourceText || undefined,
                      url: sourceUrl || undefined,
                  }
                : null,
        }
    }, [selectedTypes, sourceCode, sourceText, sourceUrl])

    const handleGenerate = async () => {
        await generate(requestPayload)
        onRunSaved()
    }

    const tabs: Tab[] = run
        ? [
              {
                  key: 'serial',
                  label: 'Serial Reference',
                  content: <CodeBlock code={run.serialReference} />,
              },
              {
                  key: 'parallel',
                  label: 'Parallel Version',
                  content: <CodeBlock code={run.parallelVersion} />,
              },
              {
                  key: 'harness',
                  label: 'Test Harness',
                  content: <CodeBlock code={run.testHarness} />,
              },
              {
                  key: 'report',
                  label: 'Report',
                  content: <ReportPanel run={run} taxonomy={taxonomy} />,
              },
          ]
        : []

    return (
        <div className="lab-page">
            <h1 className="lab-page__title">Lab</h1>
            <p className="lab-page__subtitle">
                Request a benchmark with deliberately planted concurrency
                failures.
            </p>

            <div className="lab-page__layout">
                <div className="lab-page__steps">
                    <div>
                        <div className="lab-page__step-heading">
                            <NumberBadge n={1} />
                            <h3>
                                Provide source material{' '}
                                <span
                                    style={{
                                        fontWeight: 400,
                                        color: 'var(--color-text-faint)',
                                    }}
                                >
                                    (optional)
                                </span>
                            </h3>
                        </div>
                        <p
                            className="lab-page__step-hint"
                            style={{ marginLeft: 36 }}
                        >
                            Fill in any combination — a starting program,
                            instructions for what to build, or a reference link.
                        </p>
                        <div className="lab-page__step-body">
                            <div className="lab-page__field">
                                <label className="lab-page__field-label">
                                    <CodeIcon /> Code
                                </label>
                                <textarea
                                    className="lab-page__textarea"
                                    value={sourceCode}
                                    onChange={(e) =>
                                        setSourceCode(e.target.value)
                                    }
                                    rows={4}
                                    placeholder={
                                        'for (int i = 0; i < n; i++) {\n    sum += a[i];\n}'
                                    }
                                />
                            </div>
                            <div className="lab-page__field">
                                <label className="lab-page__field-label">
                                    <TextIcon /> Text
                                </label>
                                <input
                                    className="lab-page__input"
                                    type="text"
                                    value={sourceText}
                                    onChange={(e) =>
                                        setSourceText(e.target.value)
                                    }
                                    placeholder="e.g. a parallel matrix multiply for a second-year systems course"
                                />
                            </div>
                            <div className="lab-page__field">
                                <label className="lab-page__field-label">
                                    <LinkIcon /> URL
                                </label>
                                <input
                                    className="lab-page__input"
                                    type="url"
                                    value={sourceUrl}
                                    onChange={(e) =>
                                        setSourceUrl(e.target.value)
                                    }
                                    placeholder="https://…"
                                />
                            </div>
                        </div>
                    </div>

                    <div>
                        <div className="lab-page__step-heading">
                            <NumberBadge n={2} />
                            <h3>Language</h3>
                        </div>
                        <div className="lab-page__step-body">
                            <span className="lab-page__language-value">
                                C/C++ with OpenMP
                            </span>
                        </div>
                    </div>

                    <div>
                        <div className="lab-page__step-heading">
                            <NumberBadge n={3} />
                            <h3>Select failures</h3>
                        </div>
                        <div className="lab-page__step-body">
                            <div className="lab-page__selection-header">
                                <p
                                    className="lab-page__step-hint"
                                    style={{ margin: 0 }}
                                >
                                    Select any number. None selected means the
                                    generator's choice.
                                </p>
                                <div className="lab-page__selection-header-actions">
                                    <span
                                        style={{
                                            color: 'var(--color-text-faint)',
                                        }}
                                    >
                                        {selectedTypes.length} of{' '}
                                        {allTypeKeys.length} selected
                                    </span>
                                    <button
                                        type="button"
                                        onClick={selectAll}
                                        style={{ color: 'var(--color-accent)' }}
                                    >
                                        Select all
                                    </button>
                                    <button
                                        type="button"
                                        onClick={deselectAll}
                                        style={{
                                            color: 'var(--color-text-faint)',
                                        }}
                                    >
                                        Deselect all
                                    </button>
                                </div>
                            </div>
                            <FailureCategorySelector
                                taxonomy={taxonomy}
                                selectedTypes={selectedTypes}
                                onToggle={toggleType}
                                expandedKeys={expandedKeys}
                                onToggleExpand={toggleExpand}
                            />
                        </div>
                    </div>

                    <div>
                        <div className="lab-page__step-heading">
                            <NumberBadge n={4} />
                            <button
                                type="button"
                                className="lab-page__request-toggle"
                                onClick={() => setShowRequest((v) => !v)}
                            >
                                <ChevronIcon open={showRequest} />
                                {showRequest ? 'Hide' : 'Show'} request preview
                            </button>
                        </div>
                        {showRequest && (
                            <div className="lab-page__step-body">
                                <JsonBlock value={requestPayload} />
                            </div>
                        )}
                        {generateError && (
                            <div className="lab-page__step-body">
                                <div className="lab-page__notice">
                                    {generateError}
                                </div>
                            </div>
                        )}
                    </div>
                </div>

                <div className="lab-page__sidebar">
                    <div className="lab-page__sidebar-card">
                        <p className="lab-page__sidebar-eyebrow">
                            What happens next
                        </p>
                        <p className="lab-page__sidebar-body">
                            A chain of agents builds a serial reference, plants
                            your selected failure modes into a parallel version,
                            writes a harness that tells them apart, and hands
                            all three to a separate agent that reports what it
                            finds — without seeing what was planted.
                        </p>
                    </div>
                    <PrimaryButton onClick={handleGenerate} disabled={loading}>
                        {loading ? 'Generating…' : 'Generate'}
                    </PrimaryButton>
                </div>
            </div>

            {run && (
                <div className="lab-page__results">
                    <div className="lab-page__results-header">
                        <div className="lab-page__results-title">
                            <NumberBadge n={5} />
                            <h2>Result</h2>
                        </div>
                        <a
                            href={runDownloadUrl(run.id)}
                            className="btn btn-ghost"
                        >
                            Download .zip
                        </a>
                    </div>
                    <div className="lab-page__results-panel">
                        <Tabs tabs={tabs} />
                    </div>
                </div>
            )}
        </div>
    )
}

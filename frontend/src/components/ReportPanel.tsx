import { useState } from 'react'
import type { Run } from '../types/run'
import type { Taxonomy } from '../types/taxonomy'
import { labelForType } from '../lib/taxonomyLookup'
import { ChevronIcon } from './icons'
import { PrimaryButton } from './PrimaryButton'
import { GhostButton } from './GhostButton'
import { CodeBlock } from './CodeBlock'
import './ReportPanel.css'

interface ReportPanelProps {
    run: Run
    taxonomy: Taxonomy
    onFix: () => void
    fixing: boolean
}

function formatLines(lines: number[]): string {
    return lines.length === 1 ? `Line ${lines[0]}` : `Lines ${lines.join(', ')}`
}

// The point of this panel: show what the blind analyser found before revealing what
// was actually planted, so the "guess before you look" moment survives into the real
// product, not just the mock's canned explanation. See DESIGN_NOTES.md.
export function ReportPanel({
    run,
    taxonomy,
    onFix,
    fixing,
}: ReportPanelProps) {
    const [revealed, setRevealed] = useState(false)
    const plantedKeys = new Set(run.plantedBugs.map((bug) => bug.typeKey))
    const unmatchedFindings = run.report.filter(
        (finding) => !plantedKeys.has(finding.typeKey),
    )

    return (
        <div className="report">
            <section>
                <h3 className="report__heading">What the analyser found</h3>
                <p className="report__subtext">
                    Written by an agent that never saw what was planted — it
                    only had the serial reference, the parallel version, and the
                    test harness.
                </p>
                <ul className="report__findings">
                    {run.report.map((finding, i) => (
                        <li key={i} className="report__finding">
                            <div className="report__finding-header">
                                <strong>
                                    {labelForType(taxonomy, finding.typeKey)}
                                </strong>
                                <span className="report__lines">
                                    {formatLines(finding.lines)}
                                </span>
                            </div>
                            <p>{finding.explanation}</p>
                        </li>
                    ))}
                </ul>
            </section>

            <button
                type="button"
                className="report__reveal-toggle"
                onClick={() => setRevealed((v) => !v)}
            >
                <ChevronIcon open={revealed} />
                {revealed ? 'Hide' : 'Reveal'} what was actually planted
            </button>

            {revealed && (
                <section className="report__planted">
                    <h3 className="report__heading">
                        What was actually planted
                    </h3>
                    <ul className="report__findings">
                        {run.plantedBugs.map((bug, i) => {
                            const found = run.report.some(
                                (finding) => finding.typeKey === bug.typeKey,
                            )
                            return (
                                <li key={i} className="report__finding">
                                    <div className="report__finding-header">
                                        <strong>
                                            {labelForType(
                                                taxonomy,
                                                bug.typeKey,
                                            )}
                                        </strong>
                                        <span
                                            className={
                                                found
                                                    ? 'report__match-tag report__match-tag--found'
                                                    : 'report__match-tag report__match-tag--missed'
                                            }
                                        >
                                            {found
                                                ? 'Found by the analyser'
                                                : 'Missed by the analyser'}
                                        </span>
                                    </div>
                                    <p>{bug.implementationNote}</p>
                                </li>
                            )
                        })}
                    </ul>
                    {unmatchedFindings.length > 0 && (
                        <p className="report__note">
                            The analyser also flagged something that doesn't
                            match anything planted — worth a look regardless.
                        </p>
                    )}
                </section>
            )}

            <div className="report__fix">
                {!run.fixed ? (
                    <PrimaryButton onClick={onFix} disabled={fixing}>
                        {fixing ? 'Fixing…' : 'Fix Bugs'}
                    </PrimaryButton>
                ) : (
                    <GhostButton disabled>Fixed ✓</GhostButton>
                )}
            </div>

            {run.fixed && (
                <div className="report__fixed-code">
                    <h3 className="report__heading">Fixed code</h3>
                    <CodeBlock code={run.parallelVersionFixed} />
                </div>
            )}
        </div>
    )
}

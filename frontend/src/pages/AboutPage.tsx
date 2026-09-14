import type { Taxonomy } from '../types/taxonomy'
import { colorForCategory } from '../lib/taxonomyLookup'
import './AboutPage.css'

interface AboutPageProps {
    taxonomy: Taxonomy
}

export function AboutPage({ taxonomy }: AboutPageProps) {
    return (
        <div className="about-page">
            <h1 className="about-page__title">About ThreadBare</h1>
            <p className="about-page__lead">
                ThreadBare generates parallel programs with deliberate
                concurrency bugs, for teaching. Give it a starting point —
                source code, a URL, or a plain-English description — pick which
                failure modes to plant, and a chain of agents returns four
                artifacts: a serial reference, a parallel version carrying the
                bugs, a test harness, and a report.
            </p>

            <h2 className="about-page__heading">The blind analyser</h2>
            <p className="about-page__body">
                The report is written by a separate agent that never sees what
                was planted — only the serial reference, the parallel version,
                and the harness, the same evidence a student gets. It's
                structured JSON, not prose: a list of findings, each with a
                failure type, the line or lines it points at, and a short
                explanation. That makes it a finding, not a transcript: a
                planted bug the analyser misses is either well hidden or not
                actually present, and something it flags that nobody planted is
                a real defect worth looking at.
            </p>

            <h2 className="about-page__heading">The taxonomy</h2>
            <div className="about-page__categories">
                {taxonomy.categories.map((category) => (
                    <div
                        key={category.key}
                        className="about-page__category-card"
                        style={{
                            borderLeft: `4px solid ${colorForCategory(category.key)}`,
                        }}
                    >
                        <p>{category.label}</p>
                        <p>{category.blurb}</p>
                    </div>
                ))}
            </div>

            <h2 className="about-page__heading">What each run gives you</h2>
            <ul className="about-page__list">
                <li>
                    A serial reference implementation — the oracle everything
                    else is checked against.
                </li>
                <li>
                    A parallel version carrying the planted bugs, free of any
                    comment pointing at them.
                </li>
                <li>A test harness that tells the two apart.</li>
                <li>A report from an agent that never saw what was planted.</li>
                <li>
                    A one-click "Fix Bugs" reveal for the corrected program.
                </li>
                <li>A downloadable .zip with all of the above.</li>
            </ul>

            <h2 className="about-page__heading">Scope</h2>
            <p className="about-page__body">
                Shared-memory multithreading only, C/C++ with OpenMP, for this
                version. Runs are held in the backend's memory and are lost when
                it restarts — there is no database yet.
            </p>
        </div>
    )
}

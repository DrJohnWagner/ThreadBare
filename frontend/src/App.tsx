import { useState } from 'react'
import { useTaxonomy } from './hooks/useTaxonomy'
import { LabPage } from './pages/LabPage'
import { HistoryPage } from './pages/HistoryPage'
import { DashboardPage } from './pages/DashboardPage'
import { AboutPage } from './pages/AboutPage'
import { Logo } from './components/icons'
import './App.css'

type NavKey = 'lab' | 'history' | 'dashboard' | 'about'

const NAV_ITEMS: { key: NavKey; label: string }[] = [
    { key: 'lab', label: 'Lab' },
    { key: 'history', label: 'History' },
    { key: 'dashboard', label: 'Dashboard' },
    { key: 'about', label: 'About' },
]

function App() {
    const [nav, setNav] = useState<NavKey>('lab')
    const [dataRefresh, setDataRefresh] = useState(0)
    const {
        taxonomy,
        loading: taxonomyLoading,
        error: taxonomyError,
    } = useTaxonomy()

    const bumpDataRefresh = () => setDataRefresh((n) => n + 1)

    return (
        <div className="app-shell">
            <header className="app-shell__header">
                <div className="app-shell__header-inner">
                    <div className="app-shell__brand">
                        <Logo /> ThreadBare
                    </div>
                    <nav className="app-shell__nav">
                        {NAV_ITEMS.map((item) => (
                            <button
                                key={item.key}
                                type="button"
                                className={
                                    nav === item.key
                                        ? 'app-shell__nav-item app-shell__nav-item--active'
                                        : 'app-shell__nav-item'
                                }
                                onClick={() => setNav(item.key)}
                            >
                                {item.label}
                            </button>
                        ))}
                    </nav>
                </div>
            </header>

            {taxonomyLoading && <p className="app-shell__loading">Loading…</p>}
            {taxonomyError && (
                <div className="app-shell__error-banner">
                    <div className="app-shell__error-banner-inner">
                        {taxonomyError}
                    </div>
                </div>
            )}

            {taxonomy && (
                <>
                    {nav === 'lab' && (
                        <LabPage
                            taxonomy={taxonomy}
                            onRunSaved={bumpDataRefresh}
                        />
                    )}
                    {nav === 'history' && (
                        <HistoryPage
                            taxonomy={taxonomy}
                            refreshToken={dataRefresh}
                        />
                    )}
                    {nav === 'dashboard' && (
                        <DashboardPage
                            taxonomy={taxonomy}
                            refreshToken={dataRefresh}
                        />
                    )}
                    {nav === 'about' && <AboutPage taxonomy={taxonomy} />}
                </>
            )}
        </div>
    )
}

export default App

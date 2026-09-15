import { useState } from 'react'
import './App.css'

const API_BASE = 'http://127.0.0.1:8000'

function App() {
    const [name, setName] = useState('')
    const [result, setResult] = useState('')
    const [error, setError] = useState('')
    const [loading, setLoading] = useState(false)

    async function callEndpoint(path: string) {
        setError('')
        setResult('')
        setLoading(true)
        try {
            const response = await fetch(`${API_BASE}${path}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name }),
            })
            const data = await response.json()
            if (!response.ok) {
                throw new Error(
                    typeof data.detail === 'string'
                        ? data.detail
                        : JSON.stringify(data.detail),
                )
            }
            setResult(data.message)
        } catch (err) {
            setError(
                err instanceof Error ? err.message : 'Something went wrong',
            )
        } finally {
            setLoading(false)
        }
    }

    return (
        <div className="App">
            <header className="App-header">
                <h1>ThreadBare backend test</h1>
                <input
                    type="text"
                    placeholder="Enter a name"
                    value={name}
                    onChange={(event) => setName(event.target.value)}
                />
                <div className="App-buttons">
                    <button
                        type="button"
                        onClick={() => callEndpoint('/api/greet')}
                        disabled={loading}
                    >
                        Greet
                    </button>
                    <button
                        type="button"
                        onClick={() => callEndpoint('/api/poem')}
                        disabled={loading}
                    >
                        Write a poem
                    </button>
                </div>
                {loading && <p>Loading...</p>}
                {error && <p className="App-error">Error: {error}</p>}
                {result && <p className="App-result">{result}</p>}
            </header>
        </div>
    )
}

export default App

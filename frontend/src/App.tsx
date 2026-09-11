import { useState } from 'react'
import reactLogo from './assets/react.svg'
import './App.css'

function App() {
    const [count, setCount] = useState(0)

    return (
        <div className="App">
            <header className="App-header">
                <img src={reactLogo} className="App-logo" alt="logo" />
                <p>
                    Edit <code>src/App.tsx</code> and save to reload.
                </p>
                <button
                    type="button"
                    onClick={() => setCount((count) => count + 1)}
                >
                    count is {count}
                </button>
            </header>
        </div>
    )
}

export default App

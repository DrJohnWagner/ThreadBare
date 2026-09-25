import './CodeBlock.css'

const KEYWORDS = new Set([
    'int',
    'double',
    'float',
    'char',
    'void',
    'return',
    'for',
    'while',
    'if',
    'else',
    'include',
    'define',
    'struct',
    'const',
    'static',
    'sizeof',
    'long',
])

interface Token {
    text: string
    color: string
    italic?: boolean
}

function highlightLine(line: string): Token[] {
    const tokens: Token[] = []
    const push = (text: string, color: string, italic?: boolean) =>
        tokens.push({ text, color, italic })

    if (/^\s*\/\//.test(line)) {
        return [{ text: line, color: 'var(--color-text-faint)', italic: true }]
    }

    if (/^\s*#/.test(line)) {
        const m = line.match(/^(\s*#\w+)(.*)$/)
        if (m) {
            push(m[1], '#b5591f')
            const rest = m[2]
            const inc = rest.match(/^(\s*)(<[^>]*>|"[^"]*")(.*)$/)
            if (inc) {
                push(inc[1], 'var(--color-text)')
                push(inc[2], 'var(--color-syntax-string)')
                push(inc[3], 'var(--color-text)')
            } else {
                push(rest, 'var(--color-text)')
            }
            return tokens
        }
    }

    const re = /(\d+(?:\.\d+)?)|([A-Za-z_]\w*)|("[^"]*")/g
    let last = 0
    let match: RegExpExecArray | null
    while ((match = re.exec(line)) !== null) {
        if (match.index > last)
            push(line.slice(last, match.index), 'var(--color-text)')
        const [full, num, word, str] = match
        if (num) push(full, 'var(--color-syntax-number)')
        else if (word)
            push(full, KEYWORDS.has(word) ? '#1b5fa8' : 'var(--color-text)')
        else if (str) push(full, 'var(--color-syntax-string)')
        last = match.index + full.length
    }
    if (last < line.length) push(line.slice(last), 'var(--color-text)')
    return tokens.length ? tokens : [{ text: line, color: 'var(--color-text)' }]
}

interface CodeBlockProps {
    code: string
}

export function CodeBlock({ code }: CodeBlockProps) {
    const lines = code.split('\n')
    return (
        <div className="code-block">
            <div className="code-block__scroll">
                <table>
                    <tbody>
                        {lines.map((line, idx) => (
                            <tr key={idx}>
                                <td className="code-block__line-number">
                                    {idx + 1}
                                </td>
                                <td className="code-block__line-content">
                                    {highlightLine(line).map((t, i) => (
                                        <span
                                            key={i}
                                            style={{
                                                color: t.color,
                                                fontStyle: t.italic
                                                    ? 'italic'
                                                    : 'normal',
                                            }}
                                        >
                                            {t.text}
                                        </span>
                                    ))}
                                </td>
                            </tr>
                        ))}
                    </tbody>
                </table>
            </div>
        </div>
    )
}

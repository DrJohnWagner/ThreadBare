import './CodeBlock.css'

interface JsonBlockProps {
    value: unknown
}

export function JsonBlock({ value }: JsonBlockProps) {
    return (
        <pre
            className="code-block"
            style={{
                padding: '12px 16px',
                fontSize: 12,
                color: 'var(--color-text-muted)',
            }}
        >
            {JSON.stringify(value, null, 2)}
        </pre>
    )
}

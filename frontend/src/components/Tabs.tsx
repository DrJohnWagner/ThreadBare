import { useState, type ReactNode } from 'react'
import './Tabs.css'

export interface Tab {
    key: string
    label: ReactNode
    content: ReactNode
}

interface TabsProps {
    tabs: Tab[]
}

export function Tabs({ tabs }: TabsProps) {
    const [activeKey, setActiveKey] = useState(tabs[0]?.key)
    const activeTab = tabs.find((tab) => tab.key === activeKey) ?? tabs[0]

    return (
        <div>
            <div className="tabs__bar">
                {tabs.map((tab) => (
                    <button
                        key={tab.key}
                        type="button"
                        className={
                            tab.key === activeTab?.key
                                ? 'tabs__tab tabs__tab--active'
                                : 'tabs__tab'
                        }
                        onClick={() => setActiveKey(tab.key)}
                    >
                        {tab.label}
                    </button>
                ))}
            </div>
            <div className="tabs__panel">{activeTab?.content}</div>
        </div>
    )
}

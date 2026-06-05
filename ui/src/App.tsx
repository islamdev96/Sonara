import React, { useState, useEffect, useRef } from 'react'
import './App.css'

function App() {
  const [isOn, setIsOn] = useState(true);
  const [masterBoost, setMasterBoost] = useState(150);
  
  const defaultEq = [50, 50, 50, 50, 50, 50, 50, 50, 50];
  const [eqValues, setEqValues] = useState([50, 45, 60, 75, 65, 55, 45, 50, 55]);
  
  const [clarity, setClarity] = useState(50);
  const [ambience, setAmbience] = useState(30);
  const [dynamicBoost, setDynamicBoost] = useState(70);
  const [bassBoost, setBassBoost] = useState(40);

  const [customPresets, setCustomPresets] = useState<any>({});
  const [presetName, setPresetName] = useState('');
  const [audioLevel, setAudioLevel] = useState(0);

  const bands = ["115 Hz", "250 Hz", "450 Hz", "630 Hz", "1.25 kHz", "2.70 kHz", "5.30 kHz", "7.50 kHz", "13.00 kHz"];

  const defaultPresets = {
    'Default': { eq: defaultEq, boost: 100, clarity: 0, ambience: 0, bass: 0, dynamic: 0 },
    'Music': { eq: [65, 60, 50, 45, 55, 60, 65, 70, 65], boost: 120, clarity: 60, ambience: 40, bass: 70, dynamic: 50 },
    'Movies': { eq: [75, 70, 50, 40, 60, 55, 65, 75, 80], boost: 150, clarity: 70, ambience: 80, bass: 90, dynamic: 80 },
    'Gaming': { eq: [55, 45, 40, 50, 75, 80, 85, 70, 60], boost: 130, clarity: 80, ambience: 60, bass: 50, dynamic: 60 },
    'Voice': { eq: [40, 40, 50, 65, 80, 85, 75, 60, 50], boost: 110, clarity: 90, ambience: 10, bass: 20, dynamic: 30 },
  };

  const allPresets = { ...defaultPresets, ...customPresets };

  // Load custom presets on mount
  useEffect(() => {
    const saved = localStorage.getItem('customPresets');
    if (saved) {
      setCustomPresets(JSON.parse(saved));
    }

    if (window.api && window.api.onHotkeyAction) {
      window.api.onHotkeyAction((action) => {
        if (action === 'volume-up') {
          setMasterBoost(prev => Math.min(300, prev + 10));
        } else if (action === 'volume-down') {
          setMasterBoost(prev => Math.max(100, prev - 10));
        }
      });
    }

    // Simulated Visualizer Loop since desktop capture needs permission dialogs
    const visualizerInterval = setInterval(() => {
      if (isOn) {
        setAudioLevel(Math.random() * 0.5 + 0.5); // Random intensity
      } else {
        setAudioLevel(0.1);
      }
    }, 150);

    return () => clearInterval(visualizerInterval);
  }, [isOn]);

  const applyPreset = (e: React.ChangeEvent<HTMLSelectElement>) => {
    const name = e.target.value;
    // @ts-ignore
    const preset = allPresets[name];
    if (preset) {
      setEqValues(preset.eq);
      setMasterBoost(preset.boost);
      setClarity(preset.clarity);
      setAmbience(preset.ambience);
      setBassBoost(preset.bass);
      setDynamicBoost(preset.dynamic);
    }
  };

  const savePreset = () => {
    if (!presetName) return alert("Please enter a preset name");
    const newPresets = { ...customPresets, [presetName]: { eq: eqValues, boost: masterBoost, clarity, ambience, bass: bassBoost, dynamic: dynamicBoost } };
    setCustomPresets(newPresets);
    localStorage.setItem('customPresets', JSON.stringify(newPresets));
    setPresetName('');
    alert("Preset saved!");
  };

  const resetToDefault = () => {
    setEqValues([...defaultEq]);
    setMasterBoost(100);
    setClarity(0);
    setAmbience(0);
    setBassBoost(0);
    setDynamicBoost(0);
  };

  const generatePath = () => {
    const width = 100;
    const step = width / (bands.length - 1);
    
    const visualEq = eqValues.map((val, idx) => {
      let finalVal = val;
      if (idx <= 1) finalVal += (bassBoost * 0.3);
      if (idx >= 4 && idx <= 6) finalVal += (clarity * 0.2);
      
      // Add visualizer bounce
      if (isOn) {
        finalVal += (Math.random() * 10 * audioLevel);
      }

      return Math.min(100, Math.max(0, finalVal));
    });

    const points = visualEq.map((val, idx) => {
      const x = idx * step;
      const y = 100 - val; 
      return `${idx === 0 ? 'M' : 'L'} ${x} ${y}`;
    }).join(' ');

    return points;
  };

  useEffect(() => {
    if (window.api) {
      if (isOn) {
        const finalBoost = Math.min(300, masterBoost + (dynamicBoost * 0.5));
        if (window.api.setMasterBoost) window.api.setMasterBoost(finalBoost);

        const finalEq = eqValues.map((val, idx) => {
          let v = val;
          if (idx <= 1) v += (bassBoost * 0.3);
          if (idx >= 4 && idx <= 6) v += (clarity * 0.2);
          return Math.min(100, Math.max(0, v));
        });
        if (window.api.setEqBands) window.api.setEqBands(finalEq);
      } else {
        if (window.api.setMasterBoost) window.api.setMasterBoost(100);
        if (window.api.setEqBands) window.api.setEqBands(defaultEq);
      }
    }
  }, [isOn, eqValues, masterBoost, bassBoost, clarity, dynamicBoost]);

  return (
    <div className="app-container">
      <header className="header">
        <div className="brand">
          <span className="brand-icon">ılı.</span> WinAudio Booster Pro
        </div>
        
        <div className="device-selector" style={{ display: 'flex', gap: '10px' }}>
          <select className="device-dropdown" onChange={applyPreset}>
            <option value="Custom">-- Select Preset --</option>
            {Object.keys(allPresets).map(p => <option key={p} value={p}>{p}</option>)}
          </select>
          <input 
            type="text" 
            placeholder="Preset Name" 
            value={presetName}
            onChange={e => setPresetName(e.target.value)}
            style={{ background: 'rgba(0,0,0,0.4)', color: 'white', border: '1px solid #333', borderRadius: '4px', padding: '4px' }}
          />
          <button className="reset-btn" onClick={savePreset} style={{ color: 'var(--accent)' }}>Save</button>
          <button className="reset-btn" onClick={resetToDefault}>↻ Reset</button>
        </div>

        <button 
          className={`power-btn ${isOn ? 'active' : ''}`}
          onClick={() => setIsOn(!isOn)}
        >
          {isOn ? 'ON' : 'OFF'}
        </button>
      </header>

      <div className="visualizer-container">
        <div 
          className="visualizer-line" 
          style={{ 
            opacity: isOn ? 1 : 0.3,
            transform: `scaleY(${audioLevel * 3})`,
            transition: 'transform 0.1s ease-out'
          }}
        ></div>
      </div>

      <div className="controls-row">
        <div className="sidebar">
          <div className="slider-group">
            <div className="slider-header">
              <span className="slider-label">Master Volume (Ctrl+Alt+Up)</span>
              <span className="slider-value">{masterBoost}%</span>
            </div>
            <input type="range" min="100" max="300" value={masterBoost} disabled={!isOn} onChange={(e) => setMasterBoost(Number(e.target.value))} />
          </div>
          
          <div className="slider-group" style={{ marginTop: '10px' }}>
            <div className="slider-header"><span className="slider-label">Clarity</span><span className="slider-value">{clarity}</span></div>
            <input type="range" value={clarity} onChange={(e) => setClarity(Number(e.target.value))} disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header"><span className="slider-label">Ambience</span><span className="slider-value">{ambience}</span></div>
            <input type="range" value={ambience} onChange={(e) => setAmbience(Number(e.target.value))} disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header"><span className="slider-label">Dynamic Boost</span><span className="slider-value">{dynamicBoost}</span></div>
            <input type="range" value={dynamicBoost} onChange={(e) => setDynamicBoost(Number(e.target.value))} disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header"><span className="slider-label">Bass Boost</span><span className="slider-value">{bassBoost}</span></div>
            <input type="range" value={bassBoost} onChange={(e) => setBassBoost(Number(e.target.value))} disabled={!isOn} />
          </div>
        </div>

        <div className="main-eq">
          <div className="eq-container">
            <svg className="eq-svg-overlay" viewBox="0 0 100 100" preserveAspectRatio="none" style={{ opacity: isOn ? 1 : 0.3, transition: 'all 0.1s' }}>
               <path d={generatePath()} stroke="var(--accent)" strokeWidth="2" vectorEffect="non-scaling-stroke" fill="none" />
            </svg>
            
            {bands.map((freq, idx) => (
              <div className="eq-band" key={idx}>
                <div className="eq-slider-wrapper">
                  <input type="range" className="eq-slider" value={eqValues[idx]} min="0" max="100" disabled={!isOn}
                    onChange={(e) => {
                      const newVal = Number(e.target.value);
                      const newEqs = [...eqValues];
                      newEqs[idx] = newVal;
                      setEqValues(newEqs);
                    }}
                  />
                </div>
                <div className="eq-freq">{freq}</div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  )
}

export default App

import React, { useState, useEffect } from 'react'
import './App.css'

function App() {
  const [isOn, setIsOn] = useState(true);
  const [masterBoost, setMasterBoost] = useState(150);
  
  // Fake state for visual EQ curve matching FxSound
  const [eqValues, setEqValues] = useState([50, 45, 60, 75, 65, 55, 45, 50, 55]);
  
  const bands = ["115 Hz", "250 Hz", "450 Hz", "630 Hz", "1.25 kHz", "2.70 kHz", "5.30 kHz", "7.50 kHz", "13.00 kHz"];

  // Calculate dynamic SVG path based on EQ values
  // EQ box is 220px high, 150px slider length
  const generatePath = () => {
    // We map 9 points horizontally
    const width = 100; // percent
    const step = width / (bands.length - 1);
    
    const points = eqValues.map((val, idx) => {
      const x = idx * step;
      // Invert Y because SVG 0 is at top, slider 0 is at bottom
      const y = 100 - val; 
      return `${idx === 0 ? 'M' : 'L'} ${x} ${y}`;
    }).join(' ');

    return points;
  };

  const handleMasterBoostChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const val = Number(e.target.value);
    setMasterBoost(val);
    if (isOn && window.api && window.api.setMasterBoost) {
      window.api.setMasterBoost(val);
    }
  };

  // Toggle effect
  useEffect(() => {
    if (window.api && window.api.setMasterBoost) {
      if (isOn) {
        window.api.setMasterBoost(masterBoost);
      } else {
        window.api.setMasterBoost(100); // 100% is normal volume
      }
    }
  }, [isOn]);

  return (
    <div className="app-container">
      {/* Header */}
      <header className="header">
        <div className="brand">
          <span className="brand-icon">ılı.</span> WinAudio Booster Pro
        </div>
        
        <div className="device-selector">
          <select className="device-dropdown">
            <option>System Default (Speakers)</option>
            <option>Headphones (Realtek)</option>
          </select>
        </div>

        <button 
          className={`power-btn ${isOn ? 'active' : ''}`}
          onClick={() => setIsOn(!isOn)}
          title={isOn ? "Turn Off Boost" : "Turn On Boost"}
        >
          {isOn ? 'ON' : 'OFF'}
        </button>
      </header>

      {/* Visualizer */}
      <div className="visualizer-container">
        <div className={`visualizer-line ${isOn ? 'active' : ''}`}></div>
      </div>

      <div className="controls-row">
        {/* Sidebar Controls */}
        <div className="sidebar">
          <div className="slider-group">
            <div className="slider-header">
              <span className="slider-label">Master Volume</span>
              <span className="slider-value">{masterBoost}%</span>
            </div>
            <input 
              type="range" 
              min="100" 
              max="300" 
              value={masterBoost}
              disabled={!isOn}
              onChange={handleMasterBoostChange}
            />
          </div>
          
          <div className="slider-group" style={{ marginTop: '10px' }}>
            <div className="slider-header">
              <span className="slider-label">Clarity</span>
              <span className="slider-value">50</span>
            </div>
            <input type="range" defaultValue="50" disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header">
              <span className="slider-label">Ambience</span>
              <span className="slider-value">30</span>
            </div>
            <input type="range" defaultValue="30" disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header">
              <span className="slider-label">Dynamic Boost</span>
              <span className="slider-value">70</span>
            </div>
            <input type="range" defaultValue="70" disabled={!isOn} />
          </div>
          
          <div className="slider-group">
            <div className="slider-header">
              <span className="slider-label">Bass Boost</span>
              <span className="slider-value">40</span>
            </div>
            <input type="range" defaultValue="40" disabled={!isOn} />
          </div>
        </div>

        {/* Main Equalizer */}
        <div className="main-eq">
          <div className="eq-container">
            {/* Dynamic SVG Line matching EQ Sliders */}
            <svg 
              className="eq-svg-overlay"
              viewBox="0 0 100 100" 
              preserveAspectRatio="none"
              style={{ opacity: isOn ? 1 : 0.3 }}
            >
               <path 
                 d={generatePath()} 
                 stroke="var(--accent)" 
                 strokeWidth="2" 
                 vectorEffect="non-scaling-stroke"
                 fill="none" 
               />
            </svg>
            
            {bands.map((freq, idx) => (
              <div className="eq-band" key={idx}>
                <div className="eq-slider-wrapper">
                  <input 
                    type="range" 
                    className="eq-slider" 
                    value={eqValues[idx]} 
                    min="0" 
                    max="100"
                    disabled={!isOn}
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

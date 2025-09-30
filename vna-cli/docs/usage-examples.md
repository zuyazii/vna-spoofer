# VNA-CLI Usage Examples

This document provides practical examples for using the vna-cli tool in various scenarios.

## Prerequisites

Before running these examples:

1. **Hardware Setup**:
   - Connect LibreVNA device via USB
   - Ensure device is powered and recognized

2. **Driver Installation**:
   - **Windows**: Install WinUSB driver using Zadig tool
   - **Linux**: Configure udev rules for USB permissions
   - **macOS**: No special driver needed

3. **Calibration File**:
   - Perform calibration in LibreVNA GUI first
   - Save calibration to a .cal file
   - Note the calibration frequency range

## Basic Examples

### 1. Simple S-Parameter Sweep

Perform a basic sweep from 1 MHz to 3 GHz with default settings:

```bash
vna-cli --cal my_calibration.cal \
        --fstart 1000000 \
        --fstop 3000000000 \
        --points 501
```

**Expected Output**:
```
=== LibreVNA CLI Tool ===
Frequency range: 1.0 - 3000.0 MHz
Points: 501
IFBW: 1000 Hz
Power: -10 dBm
Threshold: -10.0 dB

Connecting to LibreVNA device...
Connected to: LibreVNA (Serial: ABC123)
Loading calibration file: my_calibration.cal
Calibration loaded successfully
Configuring VNA...
Starting VNA sweep...
Progress: 50 / 501 (9.98%)
Progress: 100 / 501 (19.96%)
...
Progress: 501 / 501 (100.00%)
Processing measurements...
Applying calibration...
Evaluating pass/fail criteria...

=== Test Results ===
S11: PASS (max: -15.32 dB)
S21: PASS (max: -2.45 dB)
S12: PASS (max: -2.48 dB)
S22: PASS (max: -14.87 dB)

Overall: PASS

Cleaning up...
```

**Exit Code**: 0 (success)

---

### 2. High-Resolution Measurement

For detailed measurements with narrow IF bandwidth:

```bash
vna-cli --cal filter_cal.cal \
        --fstart 100000000 \
        --fstop 1000000000 \
        --points 1001 \
        --ifbw 100 \
        --power -20
```

**Use Case**: Filter characterization requiring high frequency resolution

**Notes**:
- Smaller IFBW (100 Hz) = better selectivity but slower sweep
- More points (1001) = better resolution
- Lower power (-20 dBm) = less distortion for sensitive DUTs

---

### 3. Logarithmic Frequency Sweep

Cover wide frequency range with more points at lower frequencies:

```bash
vna-cli --cal wideband.cal \
        --fstart 1000000 \
        --fstop 10000000000 \
        --points 201 \
        --logsweep
```

**Use Case**: Antenna or amplifier characterization over decades

**Benefits**:
- More measurement density at lower frequencies
- Covers wide range efficiently
- Useful for broadband components

---

### 4. Custom Pass/Fail Threshold

Test component against stricter specification:

```bash
vna-cli --cal component.cal \
        --fstart 500000000 \
        --fstop 2000000000 \
        --points 501 \
        --threshold-db -20
```

**Use Case**: Quality control with -20 dB return loss requirement

**Exit Codes**:
- 0: All S-parameters ≤ -20 dB (pass)
- 1: One or more > -20 dB (fail)

---

### 5. Connect to Specific Device

When multiple LibreVNA devices are connected:

```bash
vna-cli --serial 12345678 \
        --cal device1.cal \
        --fstart 1000000 \
        --fstop 3000000000
```

**Use Case**: Production line with multiple test stations

---

### 6. Save Results to JSON

Generate machine-readable output for automation:

```bash
vna-cli --cal test.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 501 \
        --json results.json
```

**Output File** (`results.json`):
```json
{
  "configuration": {
    "freqStart": 1000000,
    "freqStop": 3000000000,
    "points": 501,
    "thresholdDb": -10.0
  },
  "results": {
    "overallPass": true,
    "parameters": [
      {"parameter": "S11", "pass": true, "maxDb": -15.32},
      {"parameter": "S21", "pass": true, "maxDb": -2.45},
      {"parameter": "S12", "pass": true, "maxDb": -2.48},
      {"parameter": "S22", "pass": true, "maxDb": -14.87}
    ]
  },
  "data": [
    {"frequency": 1000000, "s11_db": -25.5, "s21_db": -3.2, ...},
    ...
  ]
}
```

**Use Case**: Integration with automated test systems, data logging

---

### 7. Save Results to CSV

Generate spreadsheet-compatible output:

```bash
vna-cli --cal test.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 501 \
        --csv results.csv
```

**Output File** (`results.csv`):
```csv
Frequency (Hz),S11 (dB),S21 (dB),S12 (dB),S22 (dB)
1000000,-25.532,-3.245,-3.287,-26.123
1005990,-25.487,-3.241,-3.283,-26.089
...
```

**Use Case**: Import into Excel, MATLAB, Python for plotting/analysis

---

### 8. Large Sweep with Automatic Segmentation

Measure with more points than device supports:

```bash
vna-cli --cal wideband.cal \
        --fstart 1000000 \
        --fstop 10000000000 \
        --points 2001
```

**Console Output**:
```
Warning: Points (2001) exceeds device max (1024)
Sweep will be segmented
Configured 2 segments
Starting segment 1 / 2
...
Starting segment 2 / 2
...
```

**Notes**:
- Automatically splits into multiple sweeps
- Seamlessly merges results
- No user intervention required

---

### 9. Fast Sweep for Quick Tests

Quick measurement with minimal dwell time:

```bash
vna-cli --cal quick.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 101 \
        --ifbw 10000 \
        --dwell 0.0001
```

**Use Case**: Rapid go/no-go testing in production

**Speedup Factors**:
- Fewer points (101 vs 501)
- Wider IFBW (10 kHz vs 1 kHz)
- Minimal dwell time (0.1 ms)

---

### 10. Custom Timeout

For slow measurements (narrow IFBW, many points):

```bash
vna-cli --cal slow.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 2001 \
        --ifbw 10 \
        --timeout-s 600
```

**Use Case**: High-resolution measurements that take several minutes

**Notes**:
- Default timeout is 120 seconds
- Increase for slow sweeps to avoid premature termination

---

## Integration Examples

### Automated Test Script (Bash)

```bash
#!/bin/bash
# automated_test.sh - Run VNA test and handle results

SERIAL="12345678"
CAL_FILE="/path/to/calibration.cal"
OUTPUT_DIR="/test/results"

# Run test
vna-cli --serial "$SERIAL" \
        --cal "$CAL_FILE" \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 501 \
        --json "$OUTPUT_DIR/result_$(date +%Y%m%d_%H%M%S).json"

# Check exit code
if [ $? -eq 0 ]; then
    echo "TEST PASSED"
    # Continue with next test or mark device as good
    exit 0
else
    echo "TEST FAILED"
    # Log failure, alert operator, etc.
    exit 1
fi
```

---

### Python Integration

```python
#!/usr/bin/env python3
import subprocess
import json
import sys

def run_vna_test(cal_file, fstart, fstop, points):
    """Run VNA CLI and return results"""
    
    cmd = [
        'vna-cli',
        '--cal', cal_file,
        '--fstart', str(fstart),
        '--fstop', str(fstop),
        '--points', str(points),
        '--json', 'temp_results.json'
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Parse JSON output
    with open('temp_results.json', 'r') as f:
        data = json.load(f)
    
    return {
        'passed': result.returncode == 0,
        'results': data
    }

# Example usage
result = run_vna_test(
    cal_file='calibration.cal',
    fstart=1e6,
    fstop=3e9,
    points=501
)

if result['passed']:
    print("Test PASSED")
    print(f"S11 max: {result['results']['results']['parameters'][0]['maxDb']} dB")
else:
    print("Test FAILED")
    sys.exit(1)
```

---

### CI/CD Integration (Jenkins)

```groovy
pipeline {
    agent any
    
    stages {
        stage('VNA Test') {
            steps {
                script {
                    def result = sh(
                        script: '''
                            vna-cli --cal calibration.cal \
                                    --fstart 1e6 \
                                    --fstop 3e9 \
                                    --points 501 \
                                    --json results.json
                        ''',
                        returnStatus: true
                    )
                    
                    if (result == 0) {
                        echo "VNA test passed"
                        currentBuild.result = 'SUCCESS'
                    } else {
                        echo "VNA test failed"
                        currentBuild.result = 'FAILURE'
                    }
                    
                    // Archive results
                    archiveArtifacts artifacts: 'results.json'
                }
            }
        }
    }
}
```

---

## Troubleshooting Examples

### Device Not Found

```bash
# List available devices first
lsusb | grep 1209:4121

# If found, check permissions (Linux)
ls -l /dev/bus/usb/001/002  # Adjust bus/device numbers

# Try with sudo
sudo vna-cli --cal test.cal --fstart 1e6 --fstop 3e9
```

---

### Calibration File Issues

```bash
# Verify calibration file exists and is readable
ls -lh calibration.cal
file calibration.cal

# Use absolute path
vna-cli --cal /full/path/to/calibration.cal --fstart 1e6 --fstop 3e9
```

---

### Timeout Issues

```bash
# Increase timeout for slow measurements
vna-cli --cal test.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 2001 \
        --ifbw 10 \
        --timeout-s 600
```

---

## Best Practices

1. **Always use appropriate calibration**:
   - Match calibration frequency range to sweep range
   - Recalibrate if hardware changes
   - Store multiple calibrations for different configurations

2. **Choose appropriate IFBW**:
   - Narrower = better selectivity, slower
   - Wider = faster, less selectivity
   - 1 kHz is good default for most applications

3. **Optimize point count**:
   - More points = better resolution, slower
   - 501 points is standard for most measurements
   - Use logarithmic sweep for wide ranges

4. **Set realistic thresholds**:
   - -10 dB is common for return loss
   - Adjust based on specification
   - Different thresholds for different parameters if needed

5. **Handle segmentation**:
   - Tool handles automatically
   - May take longer due to multiple sweeps
   - Consider reducing points if speed is critical

6. **Save results for later analysis**:
   - Always use --json or --csv for automation
   - Include timestamp in filename
   - Archive for quality records

7. **Test your test**:
   - Verify against LibreVNA GUI first
   - Use known-good DUT for baseline
   - Compare CLI vs GUI results

---

## Performance Tips

### Fast Measurements
```bash
# Minimal settings for speed
vna-cli --cal quick.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 101 \
        --ifbw 10000 \
        --dwell 0.0001
```
**Time**: ~10 seconds

### Balanced Measurements
```bash
# Standard settings (default)
vna-cli --cal standard.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 501 \
        --ifbw 1000
```
**Time**: ~30-60 seconds

### High-Precision Measurements
```bash
# Maximum quality
vna-cli --cal precision.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --points 2001 \
        --ifbw 10 \
        --dwell 0.01
```
**Time**: 5-10 minutes

---

## Support

For additional help:
- Check the main README.md
- Review implementation-plan.md
- Consult LibreVNA documentation
- Report issues on GitHub

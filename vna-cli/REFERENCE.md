# VNA-CLI Quick Reference

## Build & Install

```bash
# Quick build
cd vna-cli && ./build.sh

# Manual build
mkdir build && cd build
cmake .. && make

# Install
sudo make install
```

## Basic Usage

```bash
# Minimal command
vna-cli --cal file.cal --fstart 1e6 --fstop 3e9

# Full options
vna-cli --cal calibration.cal \
        --serial ABC123 \
        --fstart 1000000 \
        --fstop 3000000000 \
        --points 501 \
        --ifbw 1000 \
        --power -10 \
        --threshold-db -10 \
        --json results.json \
        --csv results.csv
```

## Command-Line Options

| Short | Long | Description | Default |
|-------|------|-------------|---------|
| `-c` | `--cal` | Calibration file (required) | - |
| `-s` | `--serial` | Device serial number | First |
| | `--fstart` | Start frequency (Hz) | 1e6 |
| | `--fstop` | Stop frequency (Hz) | 3e9 |
| `-p` | `--points` | Number of points | 501 |
| | `--ifbw` | IF bandwidth (Hz) | 1000 |
| | `--power` | Output power (dBm) | -10 |
| | `--logsweep` | Logarithmic sweep | Linear |
| | `--dwell` | Dwell time (s) | 0.001 |
| `-t` | `--threshold-db` | Pass/fail threshold (dB) | -10 |
| | `--timeout-s` | Overall timeout (s) | 120 |
| | `--json` | JSON output file | - |
| | `--csv` | CSV output file | - |

## Exit Codes

- `0` - All measurements passed
- `1` - One or more measurements failed
- `2` - Invalid arguments or usage

## Example Output

```
=== LibreVNA CLI Tool ===
Frequency range: 1.0 - 3000.0 MHz
Points: 501
IFBW: 1000 Hz
Power: -10 dBm
Threshold: -10.0 dB

Connecting to LibreVNA device...
Connected to: LibreVNA (Serial: ABC123)
Loading calibration file: calibration.cal
Configuring VNA...
Starting VNA sweep...
Processing measurements...

=== Test Results ===
S11: PASS (max: -15.32 dB)
S21: PASS (max: -2.45 dB)
S12: PASS (max: -2.48 dB)
S22: PASS (max: -14.87 dB)

Overall: PASS
```

## Integration Examples

### Bash Script
```bash
#!/bin/bash
vna-cli --cal test.cal --fstart 1e6 --fstop 3e9
if [ $? -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
```

### Python
```python
import subprocess
result = subprocess.run(['vna-cli', '--cal', 'test.cal', 
                        '--fstart', '1e6', '--fstop', '3e9'])
if result.returncode == 0:
    print("PASS")
```

### CI/CD (Jenkins)
```groovy
stage('VNA Test') {
    steps {
        sh 'vna-cli --cal test.cal --fstart 1e6 --fstop 3e9'
    }
}
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Device not found | Check USB connection, drivers |
| Permission denied | Use sudo (Linux) or check driver (Windows) |
| Cal file error | Check path, format, frequency range |
| Timeout | Increase `--timeout-s`, reduce points |
| Segmentation | Automatic if points > maxPoints |

## Documentation

- **README.md** - Project overview
- **QUICKSTART.md** - Quick start guide
- **docs/implementation-plan.md** - Detailed implementation
- **docs/usage-examples.md** - Practical examples
- **docs/DEVELOPMENT.md** - Developer guide
- **docs/IMPLEMENTATION_SUMMARY.md** - What's complete

## Current Status

✅ **Phase 1 Complete**: Framework, CLI, documentation
⚠️ **Phase 2 Pending**: LibreVNA driver integration required

See `docs/IMPLEMENTATION_SUMMARY.md` for details.

## Support

- GitHub Issues: Technical questions
- Documentation: Comprehensive guides in `docs/`
- LibreVNA: https://github.com/jankae/LibreVNA

---

**Version**: 0.1.0  
**Status**: Framework Complete  
**License**: Same as LibreVNA

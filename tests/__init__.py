"""
LibreVNA CLI Test Suite

This package contains comprehensive tests for the LibreVNA CLI interface.
Tests are organized by functionality and include unit tests, integration tests,
and end-to-end tests for device communication.

Test Categories:
- Unit Tests: Test individual components in isolation
- Integration Tests: Test component interactions
- Device Tests: Test actual device communication (requires hardware)
- CLI Tests: Test command-line interface functionality

Usage:
    # Run all tests
    python -m pytest tests/
    
    # Run specific test category
    python -m pytest tests/unit/
    python -m pytest tests/integration/
    python -m pytest tests/device/
    python -m pytest tests/cli/
    
    # Run with verbose output
    python -m pytest tests/ -v
    
    # Run tests that require hardware
    python -m pytest tests/device/ --device-tests
"""






#!/usr/bin/env python3
"""
Standalone script to generate plots from existing VNA measurement JSON files.

Usage:
    python generate_plot.py <json_file_path>
    python generate_plot.py output/vna_measurement_s11_test_20250930_164522/vna_measurement_s11_test_20250930_164522.json
"""

import sys
import os
from pathlib import Path

# Add the src directory to the Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

from librevna.output.plotter import VNAPlotter


def main():
    if len(sys.argv) != 2:
        print("Usage: python generate_plot.py <json_file_path>")
        print("Example: python generate_plot.py output/vna_measurement_s11_test_20250930_164522/vna_measurement_s11_test_20250930_164522.json")
        sys.exit(1)
    
    json_file = sys.argv[1]
    
    if not os.path.exists(json_file):
        print(f"Error: File not found: {json_file}")
        sys.exit(1)
    
    # Get the directory containing the JSON file
    output_dir = os.path.dirname(json_file)
    
    # Generate the plot
    plotter = VNAPlotter()
    plot_file = plotter.plot_from_json(json_file, output_dir)
    
    if plot_file:
        print(f"Plot generated successfully: {plot_file}")
    else:
        print("Failed to generate plot")
        sys.exit(1)


if __name__ == "__main__":
    main()


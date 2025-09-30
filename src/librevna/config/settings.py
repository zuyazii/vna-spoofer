"""
Configuration Settings Management

This module implements configuration hierarchy, validation, and template
generation for VNA automation settings.
"""

import os
import json
import yaml
from typing import Dict, List, Optional, Any, Union, Type

try:
    import toml
    TOML_AVAILABLE = True
except ImportError:
    TOML_AVAILABLE = False
from dataclasses import dataclass, field
from pathlib import Path
import logging
from enum import Enum

logger = logging.getLogger(__name__)


class ConfigFormat(Enum):
    """Supported configuration file formats"""
    JSON = "json"
    YAML = "yaml"
    TOML = "toml"
    
    @classmethod
    def from_string(cls, format_str: str) -> 'ConfigFormat':
        """Create ConfigFormat from string"""
        format_str = format_str.upper()
        if format_str == 'JSON':
            return cls.JSON
        elif format_str == 'YAML':
            return cls.YAML
        elif format_str == 'TOML':
            return cls.TOML
        else:
            raise ValueError(f"Unsupported format: {format_str}")


@dataclass
class ConfigTemplate:
    """Configuration template with defaults and validation"""
    name: str
    description: str
    defaults: Dict[str, Any]
    required_fields: List[str] = field(default_factory=list)
    validation_rules: Dict[str, Any] = field(default_factory=dict)
    examples: Dict[str, Any] = field(default_factory=dict)


class ConfigHierarchy:
    """
    Configuration hierarchy manager
    
    Manages configuration loading with priority:
    CLI arguments > Environment variables > Config file > Defaults
    """
    
    def __init__(self, config_name: str = "librevna"):
        """
        Initialize configuration hierarchy
        
        Args:
            config_name: Base name for configuration
        """
        self.config_name = config_name
        self.config_paths = self._get_config_paths()
        self.env_prefix = config_name.upper()
    
    def _get_config_paths(self) -> List[Path]:
        """Get list of configuration file paths to search"""
        paths = []
        
        # Current directory
        paths.append(Path.cwd() / f"{self.config_name}.json")
        paths.append(Path.cwd() / f"{self.config_name}.yaml")
        paths.append(Path.cwd() / f"{self.config_name}.toml")
        
        # User home directory
        home = Path.home()
        paths.append(home / f".{self.config_name}" / "config.json")
        paths.append(home / f".{self.config_name}" / "config.yaml")
        paths.append(home / f".{self.config_name}" / "config.toml")
        
        # System configuration directory
        if os.name == 'nt':  # Windows
            system_config = Path(os.environ.get('PROGRAMDATA', 'C:\\ProgramData'))
        else:  # Unix-like
            system_config = Path('/etc')
        
        paths.append(system_config / self.config_name / "config.json")
        paths.append(system_config / self.config_name / "config.yaml")
        paths.append(system_config / self.config_name / "config.toml")
        
        return paths
    
    def load_config(self, defaults: Dict[str, Any] = None) -> Dict[str, Any]:
        """
        Load configuration with hierarchy
        
        Args:
            defaults: Default configuration values
            
        Returns:
            Merged configuration dictionary
        """
        config = defaults or {}
        
        # Load from files (in reverse order for proper precedence)
        for config_path in reversed(self.config_paths):
            if config_path.exists():
                try:
                    file_config = self._load_config_file(config_path)
                    config = self._merge_config(config, file_config)
                    logger.debug(f"Loaded config from: {config_path}")
                except Exception as e:
                    logger.warning(f"Failed to load config from {config_path}: {e}")
        
        # Load from environment variables
        env_config = self._load_from_environment()
        config = self._merge_config(config, env_config)
        
        return config
    
    def _load_config_file(self, file_path: Path) -> Dict[str, Any]:
        """Load configuration from file"""
        suffix = file_path.suffix.lower()
        
        with open(file_path, 'r', encoding='utf-8') as f:
            if suffix == '.json':
                return json.load(f)
            elif suffix in ['.yaml', '.yml']:
                return yaml.safe_load(f)
            elif suffix == '.toml':
                if not TOML_AVAILABLE:
                    raise ImportError("toml module not available. Install with: pip install toml")
                return toml.load(f)
            else:
                raise ValueError(f"Unsupported config file format: {suffix}")
    
    def _load_from_environment(self) -> Dict[str, Any]:
        """Load configuration from environment variables"""
        config = {}
        
        for key, value in os.environ.items():
            if key.startswith(f"{self.env_prefix}_"):
                # Convert environment variable name to config key
                config_key = key[len(f"{self.env_prefix}_"):].lower()
                config_key = config_key.replace('_', '.')
                
                # Convert value to appropriate type
                config[config_key] = self._convert_env_value(value)
        
        return config
    
    def _convert_env_value(self, value: str) -> Union[str, int, float, bool, List[str]]:
        """Convert environment variable value to appropriate type"""
        # Boolean values
        if value.lower() in ['true', 'yes', 'on', '1']:
            return True
        elif value.lower() in ['false', 'no', 'off', '0']:
            return False
        
        # Numeric values
        try:
            if '.' in value:
                return float(value)
            else:
                return int(value)
        except ValueError:
            pass
        
        # List values (comma-separated)
        if ',' in value:
            return [item.strip() for item in value.split(',')]
        
        # String value
        return value
    
    def _merge_config(self, base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
        """Merge configuration dictionaries with dot notation support"""
        result = base.copy()
        
        for key, value in override.items():
            if '.' in key:
                # Handle dot notation (e.g., "device.port" -> {"device": {"port": value}})
                self._set_nested_value(result, key, value)
            else:
                result[key] = value
        
        return result
    
    def _set_nested_value(self, config: Dict[str, Any], key: str, value: Any):
        """Set nested value using dot notation"""
        keys = key.split('.')
        current = config
        
        for k in keys[:-1]:
            if k not in current:
                current[k] = {}
            current = current[k]
        
        current[keys[-1]] = value
    
    def get_nested_value(self, config: Dict[str, Any], key: str, default: Any = None) -> Any:
        """Get nested value using dot notation"""
        keys = key.split('.')
        current = config
        
        for k in keys:
            if isinstance(current, dict) and k in current:
                current = current[k]
            else:
                return default
        
        return current


class ConfigManager:
    """
    Configuration manager with validation and template support
    
    This class provides comprehensive configuration management including
    loading, validation, and template generation.
    """
    
    def __init__(self, config_name: str = "librevna"):
        """
        Initialize configuration manager
        
        Args:
            config_name: Base name for configuration
        """
        self.config_name = config_name
        self.hierarchy = ConfigHierarchy(config_name)
        self.templates: Dict[str, ConfigTemplate] = {}
        self._register_default_templates()
    
    def _register_default_templates(self):
        """Register default configuration templates"""
        # S11 Test template
        s11_template = ConfigTemplate(
            name="s11_test",
            description="S11 test configuration",
            defaults={
                "frequency": {
                    "start": 1e6,
                    "stop": 6e9,
                    "points": 201
                },
                "thresholds": {
                    "pass_db": -10.0,
                    "fail_db": -5.0
                },
                "measurement": {
                    "ifbw": 1000,
                    "power_level": -10.0,
                    "averaging": 1
                },
                "calibration": {
                    "enabled": True,
                    "file": None
                },
                "output": {
                    "formats": ["json", "csv"],
                    "directory": "output"
                }
            },
            required_fields=["frequency.start", "frequency.stop"],
            validation_rules={
                "frequency.start": {"type": "number", "min": 1e3},
                "frequency.stop": {"type": "number", "min": 1e3},
                "frequency.points": {"type": "integer", "min": 2, "max": 10000},
                "thresholds.pass_db": {"type": "number", "max": 0},
                "thresholds.fail_db": {"type": "number", "max": 0}
            }
        )
        self.templates["s11_test"] = s11_template
        
        # Device template
        device_template = ConfigTemplate(
            name="device",
            description="Device configuration",
            defaults={
                "connection": {
                    "type": "usb",
                    "timeout": 30.0,
                    "retry_count": 3
                },
                "settings": {
                    "ifbw": 1000,
                    "power_level": -10.0,
                    "averaging": 1
                }
            },
            required_fields=["connection.type"],
            validation_rules={
                "connection.timeout": {"type": "number", "min": 1.0},
                "connection.retry_count": {"type": "integer", "min": 0, "max": 10}
            }
        )
        self.templates["device"] = device_template
    
    def load_config(self, template_name: Optional[str] = None) -> Dict[str, Any]:
        """
        Load configuration with template defaults
        
        Args:
            template_name: Template to use for defaults
            
        Returns:
            Loaded configuration
        """
        # Get defaults from template
        defaults = {}
        if template_name and template_name in self.templates:
            defaults = self.templates[template_name].defaults.copy()
        
        # Load configuration with hierarchy
        config = self.hierarchy.load_config(defaults)
        
        # Validate if template specified
        if template_name and template_name in self.templates:
            self.validate_config(config, template_name)
        
        return config
    
    def validate_config(self, config: Dict[str, Any], template_name: str) -> bool:
        """
        Validate configuration against template
        
        Args:
            config: Configuration to validate
            template_name: Template name for validation
            
        Returns:
            True if valid
            
        Raises:
            ValueError: If validation fails
        """
        if template_name not in self.templates:
            raise ValueError(f"Unknown template: {template_name}")
        
        template = self.templates[template_name]
        errors = []
        
        # Check required fields
        for field in template.required_fields:
            if self.hierarchy.get_nested_value(config, field) is None:
                errors.append(f"Required field missing: {field}")
        
        # Validate fields against rules
        for field, rules in template.validation_rules.items():
            value = self.hierarchy.get_nested_value(config, field)
            if value is not None:
                field_errors = self._validate_field(field, value, rules)
                errors.extend(field_errors)
        
        if errors:
            raise ValueError(f"Configuration validation failed: {'; '.join(errors)}")
        
        return True
    
    def _validate_field(self, field: str, value: Any, rules: Dict[str, Any]) -> List[str]:
        """Validate single field against rules"""
        errors = []
        
        # Type validation
        if "type" in rules:
            expected_type = rules["type"]
            if expected_type == "number" and not isinstance(value, (int, float)):
                errors.append(f"{field}: expected number, got {type(value).__name__}")
            elif expected_type == "integer" and not isinstance(value, int):
                errors.append(f"{field}: expected integer, got {type(value).__name__}")
            elif expected_type == "string" and not isinstance(value, str):
                errors.append(f"{field}: expected string, got {type(value).__name__}")
            elif expected_type == "boolean" and not isinstance(value, bool):
                errors.append(f"{field}: expected boolean, got {type(value).__name__}")
        
        # Range validation for numbers
        if isinstance(value, (int, float)):
            if "min" in rules and value < rules["min"]:
                errors.append(f"{field}: value {value} below minimum {rules['min']}")
            if "max" in rules and value > rules["max"]:
                errors.append(f"{field}: value {value} above maximum {rules['max']}")
        
        return errors
    
    def save_config(self, config: Dict[str, Any], file_path: Union[str, Path], 
                   format: ConfigFormat = ConfigFormat.JSON) -> None:
        """
        Save configuration to file
        
        Args:
            config: Configuration to save
            file_path: Output file path
            format: File format
        """
        file_path = Path(file_path)
        file_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            if format == ConfigFormat.JSON:
                json.dump(config, f, indent=2, ensure_ascii=False)
            elif format == ConfigFormat.YAML:
                yaml.dump(config, f, default_flow_style=False, allow_unicode=True)
            elif format == ConfigFormat.TOML:
                if not TOML_AVAILABLE:
                    raise ImportError("toml module not available. Install with: pip install toml")
                toml.dump(config, f)
            else:
                raise ValueError(f"Unsupported format: {format}")
        
        logger.info(f"Configuration saved to: {file_path}")
    
    def generate_template_config(self, template_name: str, 
                               file_path: Union[str, Path],
                               format: ConfigFormat = ConfigFormat.JSON) -> None:
        """
        Generate configuration file from template
        
        Args:
            template_name: Template name
            file_path: Output file path
            format: File format
        """
        if template_name not in self.templates:
            raise ValueError(f"Unknown template: {template_name}")
        
        template = self.templates[template_name]
        
        # Create config with defaults and examples
        config = {
            "_template": template_name,
            "_description": template.description,
            **template.defaults
        }
        
        # Add examples if available
        if template.examples:
            config["_examples"] = template.examples
        
        self.save_config(config, file_path, format)
        logger.info(f"Template configuration generated: {file_path}")
    
    def get_template_info(self, template_name: str) -> Dict[str, Any]:
        """Get information about a template"""
        if template_name not in self.templates:
            raise ValueError(f"Unknown template: {template_name}")
        
        template = self.templates[template_name]
        return {
            "name": template.name,
            "description": template.description,
            "required_fields": template.required_fields,
            "validation_rules": template.validation_rules,
            "defaults": template.defaults,
            "examples": template.examples
        }
    
    def list_templates(self) -> List[str]:
        """List available templates"""
        return list(self.templates.keys())
    
    def register_template(self, template: ConfigTemplate) -> None:
        """Register a new configuration template"""
        self.templates[template.name] = template
        logger.info(f"Registered template: {template.name}")


# Convenience functions
def load_config(template_name: Optional[str] = None, 
               config_name: str = "librevna") -> Dict[str, Any]:
    """
    Load configuration with template defaults
    
    Args:
        template_name: Template to use for defaults
        config_name: Base name for configuration
        
    Returns:
        Loaded configuration
    """
    manager = ConfigManager(config_name)
    return manager.load_config(template_name)


def save_config(config: Dict[str, Any], file_path: Union[str, Path],
               format: ConfigFormat = ConfigFormat.JSON) -> None:
    """
    Save configuration to file
    
    Args:
        config: Configuration to save
        file_path: Output file path
        format: File format
    """
    manager = ConfigManager()
    manager.save_config(config, file_path, format)


def validate_config(config: Dict[str, Any], template_name: str) -> bool:
    """
    Validate configuration against template
    
    Args:
        config: Configuration to validate
        template_name: Template name for validation
        
    Returns:
        True if valid
    """
    manager = ConfigManager()
    return manager.validate_config(config, template_name)

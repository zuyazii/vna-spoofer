"""
Logging and Diagnostics

This module implements structured logging with levels, file and console output,
diagnostic information collection, and performance metrics logging.
"""

import logging
import logging.handlers
import sys
import time
import json
from typing import Dict, List, Optional, Any, Union
from dataclasses import dataclass, field
from pathlib import Path
from datetime import datetime
import threading
import traceback


@dataclass
class LogConfig:
    """Configuration for logging system"""
    level: str = "INFO"
    console_output: bool = True
    file_output: bool = True
    log_directory: str = "logs"
    max_file_size: int = 10 * 1024 * 1024  # 10 MB
    backup_count: int = 5
    format_string: str = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    date_format: str = "%Y-%m-%d %H:%M:%S"
    include_thread_id: bool = True
    include_process_id: bool = False


class StructuredLogger:
    """
    Structured logger with enhanced features
    
    This class provides structured logging capabilities with performance
    metrics, diagnostic information, and flexible output options.
    """
    
    def __init__(self, name: str, config: Optional[LogConfig] = None):
        """
        Initialize structured logger
        
        Args:
            name: Logger name
            config: Logging configuration
        """
        self.name = name
        self.config = config or LogConfig()
        self.logger = logging.getLogger(name)
        self._setup_logger()
        
        # Performance tracking
        self._performance_metrics: Dict[str, List[float]] = {}
        self._diagnostic_info: Dict[str, Any] = {}
        
        # Thread safety
        self._lock = threading.Lock()
    
    def _setup_logger(self):
        """Setup logger with handlers and formatters"""
        # Clear existing handlers
        self.logger.handlers.clear()
        
        # Set level
        level = getattr(logging, self.config.level.upper(), logging.INFO)
        self.logger.setLevel(level)
        
        # Create formatter
        formatter = logging.Formatter(
            self.config.format_string,
            datefmt=self.config.date_format
        )
        
        # Console handler
        if self.config.console_output:
            console_handler = logging.StreamHandler(sys.stdout)
            console_handler.setFormatter(formatter)
            self.logger.addHandler(console_handler)
        
        # File handler
        if self.config.file_output:
            self._setup_file_handler(formatter)
    
    def _setup_file_handler(self, formatter: logging.Formatter):
        """Setup file handler with rotation"""
        # Ensure log directory exists
        log_dir = Path(self.config.log_directory)
        log_dir.mkdir(parents=True, exist_ok=True)
        
        # Create log file path
        timestamp = datetime.now().strftime("%Y%m%d")
        log_file = log_dir / f"{self.name}_{timestamp}.log"
        
        # Create rotating file handler
        file_handler = logging.handlers.RotatingFileHandler(
            log_file,
            maxBytes=self.config.max_file_size,
            backupCount=self.config.backup_count
        )
        file_handler.setFormatter(formatter)
        self.logger.addHandler(file_handler)
    
    def debug(self, message: str, **kwargs):
        """Log debug message with optional structured data"""
        self._log(logging.DEBUG, message, **kwargs)
    
    def info(self, message: str, **kwargs):
        """Log info message with optional structured data"""
        self._log(logging.INFO, message, **kwargs)
    
    def warning(self, message: str, **kwargs):
        """Log warning message with optional structured data"""
        self._log(logging.WARNING, message, **kwargs)
    
    def error(self, message: str, **kwargs):
        """Log error message with optional structured data"""
        self._log(logging.ERROR, message, **kwargs)
    
    def critical(self, message: str, **kwargs):
        """Log critical message with optional structured data"""
        self._log(logging.CRITICAL, message, **kwargs)
    
    def _log(self, level: int, message: str, **kwargs):
        """Internal logging method with structured data"""
        # Add structured data if provided
        if kwargs:
            structured_data = json.dumps(kwargs, default=str)
            message = f"{message} | {structured_data}"
        
        # Add thread/process info if configured
        if self.config.include_thread_id or self.config.include_process_id:
            extra_info = []
            if self.config.include_thread_id:
                extra_info.append(f"TID:{threading.get_ident()}")
            if self.config.include_process_id:
                extra_info.append(f"PID:{os.getpid()}")
            
            if extra_info:
                message = f"{message} | {' '.join(extra_info)}"
        
        self.logger.log(level, message)
    
    def log_performance(self, operation: str, duration: float, **metrics):
        """
        Log performance metrics
        
        Args:
            operation: Operation name
            duration: Duration in seconds
            **metrics: Additional performance metrics
        """
        with self._lock:
            if operation not in self._performance_metrics:
                self._performance_metrics[operation] = []
            self._performance_metrics[operation].append(duration)
        
        # Log performance data
        perf_data = {
            'operation': operation,
            'duration': duration,
            **metrics
        }
        self.info(f"Performance: {operation}", **perf_data)
    
    def log_diagnostic(self, category: str, data: Dict[str, Any]):
        """
        Log diagnostic information
        
        Args:
            category: Diagnostic category
            data: Diagnostic data
        """
        with self._lock:
            self._diagnostic_info[category] = data
        
        self.debug(f"Diagnostic: {category}", **data)
    
    def log_exception(self, message: str, exc_info: bool = True, **kwargs):
        """
        Log exception with full traceback
        
        Args:
            message: Error message
            exc_info: Include exception info
            **kwargs: Additional context
        """
        if exc_info:
            kwargs['traceback'] = traceback.format_exc()
        
        self.error(message, **kwargs)
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get performance metrics summary"""
        with self._lock:
            summary = {}
            for operation, durations in self._performance_metrics.items():
                if durations:
                    summary[operation] = {
                        'count': len(durations),
                        'total': sum(durations),
                        'average': sum(durations) / len(durations),
                        'min': min(durations),
                        'max': max(durations)
                    }
            return summary
    
    def get_diagnostic_summary(self) -> Dict[str, Any]:
        """Get diagnostic information summary"""
        with self._lock:
            return self._diagnostic_info.copy()
    
    def clear_metrics(self):
        """Clear performance metrics and diagnostic info"""
        with self._lock:
            self._performance_metrics.clear()
            self._diagnostic_info.clear()


def setup_logging(config: Optional[LogConfig] = None, 
                 root_logger: bool = True) -> LogConfig:
    """
    Setup logging system
    
    Args:
        config: Logging configuration
        root_logger: Whether to configure root logger
        
    Returns:
        Logging configuration used
    """
    config = config or LogConfig()
    
    if root_logger:
        # Clear all existing handlers
        root_logger = logging.getLogger()
        root_logger.handlers.clear()
        
        # Set level
        level = getattr(logging, config.level.upper(), logging.INFO)
        root_logger.setLevel(level)
        
        # Create formatter
        formatter = logging.Formatter(
            config.format_string,
            datefmt=config.date_format
        )
        
        # Console handler (only if console_output is enabled)
        if config.console_output:
            console_handler = logging.StreamHandler(sys.stdout)
            console_handler.setFormatter(formatter)
            root_logger.addHandler(console_handler)
        
        # File handler
        if config.file_output:
            log_dir = Path(config.log_directory)
            log_dir.mkdir(parents=True, exist_ok=True)
            
            timestamp = datetime.now().strftime("%Y%m%d")
            log_file = log_dir / f"root_{timestamp}.log"
            
            file_handler = logging.handlers.RotatingFileHandler(
                log_file,
                maxBytes=config.max_file_size,
                backupCount=config.backup_count
            )
            file_handler.setFormatter(formatter)
            root_logger.addHandler(file_handler)
        
        # Configure all existing loggers to use the same handlers
        for logger_name in logging.Logger.manager.loggerDict:
            logger = logging.getLogger(logger_name)
            logger.handlers.clear()
            logger.setLevel(level)
            logger.propagate = True  # This will make it use the root logger's handlers
    
    return config


def get_logger(name: str, config: Optional[LogConfig] = None) -> StructuredLogger:
    """
    Get structured logger instance
    
    Args:
        name: Logger name
        config: Logging configuration
        
    Returns:
        Structured logger instance
    """
    return StructuredLogger(name, config)


# Performance measurement decorator
def log_performance(operation_name: Optional[str] = None):
    """
    Decorator to log function performance
    
    Args:
        operation_name: Name for the operation (defaults to function name)
        
    Returns:
        Decorated function
    """
    def decorator(func):
        def wrapper(*args, **kwargs):
            name = operation_name or func.__name__
            logger = get_logger(func.__module__)
            
            start_time = time.time()
            try:
                result = func(*args, **kwargs)
                duration = time.time() - start_time
                logger.log_performance(name, duration, status="success")
                return result
            except Exception as e:
                duration = time.time() - start_time
                logger.log_performance(name, duration, status="error", error=str(e))
                raise
        
        return wrapper
    return decorator


# Context manager for performance measurement
class PerformanceTimer:
    """Context manager for measuring operation performance"""
    
    def __init__(self, operation_name: str, logger: Optional[StructuredLogger] = None):
        """
        Initialize performance timer
        
        Args:
            operation_name: Name of the operation
            logger: Logger instance (creates new if None)
        """
        self.operation_name = operation_name
        self.logger = logger or get_logger("performance")
        self.start_time = None
    
    def __enter__(self):
        """Start timing"""
        self.start_time = time.time()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """End timing and log result"""
        if self.start_time is not None:
            duration = time.time() - self.start_time
            status = "error" if exc_type else "success"
            self.logger.log_performance(self.operation_name, duration, status=status)

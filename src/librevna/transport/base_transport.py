"""
Base transport class for LibreVNA communication

This module defines the abstract base class that all transport implementations
must inherit from. It provides a common interface for device communication
regardless of the underlying transport mechanism (USB, TCP, etc.).

The base class defines the essential methods that any transport must implement:
- connect(): Establish connection to device
- disconnect(): Close connection and cleanup resources
- send(): Send data to device
- recv(): Receive data from device
- is_connected(): Check connection status

Usage:
    from . import BaseTransport
    
    class CustomTransport(BaseTransport):
        def connect(self):
            # Implementation specific to your transport
            pass
        
        def disconnect(self):
            # Cleanup implementation
            pass
        
        def send(self, data: bytes):
            # Send implementation
            pass
        
        def recv(self, size: int = 1024) -> bytes:
            # Receive implementation
            pass
        
        def is_connected(self) -> bool:
            # Connection status check
            pass
"""

from abc import ABC, abstractmethod
from typing import Optional


class BaseTransport(ABC):
    """
    Abstract base class for LibreVNA transport implementations
    
    This class defines the interface that all transport layers must implement.
    It ensures consistent behavior across different communication methods
    (USB, TCP, etc.) and provides a common API for the device layer.
    """
    
    def __init__(self):
        """Initialize the transport layer"""
        self._connected = False
        self._device_info: Optional[dict] = None
    
    @abstractmethod
    def connect(self, **kwargs) -> bool:
        """
        Establish connection to the LibreVNA device
        
        Args:
            **kwargs: Transport-specific connection parameters
            
        Returns:
            bool: True if connection successful, False otherwise
            
        Raises:
            ConnectionError: If connection fails
        """
        pass
    
    @abstractmethod
    def disconnect(self) -> None:
        """
        Close connection and cleanup resources
        
        This method should:
        - Close any open connections
        - Release device resources
        - Reset connection state
        - Handle any cleanup operations
        """
        pass
    
    @abstractmethod
    def send(self, data: bytes) -> None:
        """
        Send data to the LibreVNA device
        
        Args:
            data: Raw bytes to send to device
            
        Raises:
            ConnectionError: If not connected or send fails
            ValueError: If data is invalid
        """
        pass
    
    @abstractmethod
    def recv(self, size: int = 1024) -> bytes:
        """
        Receive data from the LibreVNA device
        
        Args:
            size: Maximum number of bytes to receive
            
        Returns:
            bytes: Received data from device
            
        Raises:
            ConnectionError: If not connected or receive fails
        """
        pass
    
    @abstractmethod
    def is_connected(self) -> bool:
        """
        Check if transport is connected to device
        
        Returns:
            bool: True if connected, False otherwise
        """
        pass
    
    @property
    def device_info(self) -> Optional[dict]:
        """
        Get device information
        
        Returns:
            dict: Device information including VID, PID, serial, etc.
            None if not connected
        """
        return self._device_info
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - ensure cleanup"""
        self.disconnect()
    
    def __repr__(self) -> str:
        """String representation of transport"""
        status = "connected" if self._connected else "disconnected"
        return f"{self.__class__.__name__}({status})"

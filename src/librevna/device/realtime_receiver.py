"""
Real-time data receiver for LibreVNA devices

This module implements event-driven data reception from LibreVNA devices,
providing real-time packet processing and streaming measurement data.

The receiver supports:
- Event-driven packet reception using threading
- Real-time protocol parsing
- Callback-based data processing
- Error handling and recovery
- Queue-based data buffering

Usage:
    from . import RealtimeReceiver
    
    # Create receiver with callbacks
    receiver = RealtimeReceiver(
        transport=transport,
        on_measurement=lambda data: print(f"Measurement: {data}"),
        on_error=lambda error: print(f"Error: {error}")
    )
    
    # Start receiving data
    receiver.start()
    
    # Stop receiving
    receiver.stop()
"""

import threading
import queue
import time
import logging
from typing import Optional, Callable, Dict, Any, List
from ..protocol.parser import ProtocolParser
from ..protocol.exceptions import ParseError, ChecksumError

logger = logging.getLogger(__name__)


class RealtimeReceiver:
    """
    Real-time data receiver for LibreVNA devices
    
    This class provides event-driven data reception and real-time packet processing
    for LibreVNA devices. It runs in a separate thread and processes incoming data
    as it arrives from the device.
    """
    
    def __init__(self, 
                 transport,
                 on_measurement: Optional[Callable[[Dict[str, Any]], None]] = None,
                 on_error: Optional[Callable[[Exception], None]] = None,
                 on_status: Optional[Callable[[str], None]] = None,
                 buffer_size: int = 1000):
        """
        Initialize real-time receiver
        
        Args:
            transport: Transport layer for device communication
            on_measurement: Callback for measurement data (VNADatapoint)
            on_error: Callback for errors
            on_status: Callback for status updates
            buffer_size: Maximum buffer size for data queue
        """
        self.transport = transport
        self.on_measurement = on_measurement
        self.on_error = on_error
        self.on_status = on_status
        self.buffer_size = buffer_size
        
        # Threading
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()
        self._running = False
        
        # Data processing
        self._parser = ProtocolParser()
        self._data_queue = queue.Queue(maxsize=buffer_size)
        
        # Statistics
        self._packets_received = 0
        self._packets_parsed = 0
        self._errors = 0
        self._start_time: Optional[float] = None
    
    def start(self) -> None:
        """
        Start real-time data reception
        
        This method starts the background thread that continuously receives
        and processes data from the device.
        """
        if self._running:
            logger.warning("Receiver already running")
            return
        
        if not self.transport.is_connected():
            raise ConnectionError("Transport not connected")
        
        self._stop_event.clear()
        self._running = True
        self._start_time = time.time()
        
        # Start background thread
        self._thread = threading.Thread(target=self._receive_loop, daemon=True)
        self._thread.start()
        
        logger.info("Real-time receiver started")
        if self.on_status:
            self.on_status("started")
    
    def stop(self) -> None:
        """
        Stop real-time data reception
        
        This method stops the background thread and cleans up resources.
        """
        if not self._running:
            logger.warning("Receiver not running")
            return
        
        self._running = False
        self._stop_event.set()
        
        # Wait for thread to finish
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=2.0)
        
        logger.info("Real-time receiver stopped")
        if self.on_status:
            self.on_status("stopped")
    
    def is_running(self) -> bool:
        """Check if receiver is currently running"""
        return self._running and self._thread and self._thread.is_alive()
    
    def get_statistics(self) -> Dict[str, Any]:
        """
        Get receiver statistics
        
        Returns:
            Dict containing statistics about data reception and processing
        """
        duration = time.time() - self._start_time if self._start_time else 0
        
        return {
            'running': self.is_running(),
            'packets_received': self._packets_received,
            'packets_parsed': self._packets_parsed,
            'errors': self._errors,
            'duration': duration,
            'packet_rate': self._packets_received / duration if duration > 0 else 0,
            'parse_rate': self._packets_parsed / duration if duration > 0 else 0,
            'error_rate': self._errors / duration if duration > 0 else 0
        }
    
    def _receive_loop(self) -> None:
        """
        Main reception loop running in background thread
        
        This method continuously receives data from the device and processes
        it through the protocol parser.
        """
        logger.info("Starting data reception loop")
        
        try:
            while self._running and not self._stop_event.is_set():
                try:
                    # Receive data from device
                    data = self.transport.recv(1024)
                    if not data:
                        # No data available, short sleep to prevent busy waiting
                        time.sleep(0.001)
                        continue
                    
                    self._packets_received += 1
                    
                    # Process received data
                    self._process_data(data)
                    
                except Exception as e:
                    self._errors += 1
                    logger.error(f"Error in reception loop: {e}")
                    
                    if self.on_error:
                        self.on_error(e)
                    
                    # Short sleep on error to prevent tight error loop
                    time.sleep(0.01)
                    
        except Exception as e:
            logger.error(f"Fatal error in reception loop: {e}")
            if self.on_error:
                self.on_error(e)
        finally:
            self._running = False
            logger.info("Data reception loop ended")
    
    def _process_data(self, data: bytes) -> None:
        """
        Process received data through protocol parser
        
        Args:
            data: Raw data received from device
        """
        try:
            # Parse data using protocol parser
            messages = self._parser.parse(data)
            
            for message in messages:
                self._packets_parsed += 1
                self._handle_message(message)
                
        except (ParseError, ChecksumError) as e:
            self._errors += 1
            logger.warning(f"Protocol parsing error: {e}")
            
            if self.on_error:
                self.on_error(e)
                
        except Exception as e:
            self._errors += 1
            logger.error(f"Unexpected error processing data: {e}")
            
            if self.on_error:
                self.on_error(e)
    
    def _handle_message(self, message: Dict[str, Any]) -> None:
        """
        Handle parsed protocol message
        
        Args:
            message: Parsed message from protocol parser
        """
        try:
            message_type = message.get('type')
            
            if message_type == 'VNA_DATAPOINT':
                # Parse VNA measurement data
                datapoint = self._parser.parse_vna_datapoint(message['payload'])
                
                if self.on_measurement:
                    self.on_measurement(datapoint)
                    
            elif message_type == 'ERROR':
                # Handle error messages
                error_msg = message['payload'].decode('utf-8', errors='ignore')
                logger.error(f"Device error: {error_msg}")
                
                if self.on_error:
                    self.on_error(Exception(f"Device error: {error_msg}"))
                    
            elif message_type == 'INFO':
                # Handle info messages
                info_msg = message['payload'].decode('utf-8', errors='ignore')
                logger.info(f"Device info: {info_msg}")
                
                if self.on_status:
                    self.on_status(f"Info: {info_msg}")
                    
            else:
                # Handle other message types
                logger.debug(f"Received message type: {message_type}")
                
        except Exception as e:
            self._errors += 1
            logger.error(f"Error handling message: {e}")
            
            if self.on_error:
                self.on_error(e)
    
    def clear_buffer(self) -> None:
        """Clear the protocol parser buffer"""
        self._parser.clear_buffer()
    
    def get_queued_measurements(self) -> List[Dict[str, Any]]:
        """
        Get queued measurement data
        
        Returns:
            List of measurement data that was queued during reception
        """
        measurements = []
        
        try:
            while True:
                measurement = self._data_queue.get_nowait()
                measurements.append(measurement)
        except queue.Empty:
            pass
        
        return measurements


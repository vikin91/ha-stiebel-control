#!/usr/bin/env python3
"""
CAN Message Database Analysis Tool

This script provides utilities to analyze SQL databases (PostgreSQL, MySQL, SQLite)
created by the mqtt_to_database.py MQTT logger script.

Usage:
    python3 analyze_can_database.py /path/to/can_messages.db

    For PostgreSQL/MySQL, use connection string:
    python3 analyze_can_database.py "postgresql://user:pass@host/dbname"
    python3 analyze_can_database.py "mysql+pymysql://user:pass@host/dbname"

Requirements:
    pip install pandas tabulate sqlalchemy
"""

import sqlite3
import sys
import argparse
import os
from datetime import datetime, timedelta
from pathlib import Path

# Silence SQLAlchemy 2.0 deprecation warnings
os.environ['SQLALCHEMY_SILENCE_UBER_WARNING'] = '1'

try:
    import pandas as pd
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    print("Warning: pandas not available. Install with 'pip install pandas' for enhanced analysis.")

try:
    from tabulate import tabulate
    TABULATE_AVAILABLE = True
except ImportError:
    TABULATE_AVAILABLE = False

try:
    from sqlalchemy import create_engine, text
    SQLALCHEMY_AVAILABLE = True
except ImportError:
    SQLALCHEMY_AVAILABLE = False


class CanDatabaseAnalyzer:
    def __init__(self, db_path):
        self.db_path = db_path
        self.use_sqlalchemy = False
        
        # Check if it's a connection string (postgresql://, mysql://, etc)
        if '://' in db_path:
            if not SQLALCHEMY_AVAILABLE:
                print("Error: SQLAlchemy required for non-SQLite databases")
                print("Install with: sudo apt install python3-sqlalchemy")
                sys.exit(1)
            
            self.use_sqlalchemy = True
            self.engine = create_engine(db_path)
            self.conn = self.engine.connect()
        else:
            # SQLite file
            if not Path(db_path).exists():
                print(f"Error: Database file '{db_path}' not found")
                sys.exit(1)
            self.conn = sqlite3.connect(db_path)
            self.conn.row_factory = sqlite3.Row
        
    def __del__(self):
        if hasattr(self, 'conn'):
            self.conn.close()
    
    def execute_query(self, query, params=None):
        """Execute query with proper handling for both SQLite and SQLAlchemy"""
        if self.use_sqlalchemy:
            result = self.conn.execute(text(query), params or {})
            return result
        else:
            cursor = self.conn.cursor()
            if params:
                cursor.execute(query, params)
            else:
                cursor.execute(query)
            return cursor
    
    def get_summary(self):
        """Get database summary statistics"""
        cursor = self.conn.cursor()
        
        stats = {}
        
        # Total messages
        cursor.execute("SELECT COUNT(*) as total FROM can_messages")
        stats['total_messages'] = cursor.fetchone()[0]
        
        # Date range
        cursor.execute("SELECT MIN(timestamp) as first, MAX(timestamp) as last FROM can_messages")
        row = cursor.fetchone()
        stats['first_message'] = row[0]
        stats['last_message'] = row[1]
        
        # Messages by addressee
        cursor.execute("""
            SELECT addressee, COUNT(*) as count 
            FROM can_messages 
            GROUP BY addressee 
            ORDER BY count DESC
        """)
        stats['by_addressee'] = cursor.fetchall()
        
        # Unique parameter names
        cursor.execute("SELECT COUNT(DISTINCT elster_name) FROM can_messages")
        stats['unique_parameters'] = cursor.fetchone()[0]
        
        # Unknown indices
        cursor.execute("""
            SELECT COUNT(*) as count 
            FROM can_messages 
            WHERE elster_name = 'INDEX_NOT_FOUND' OR elster_name = 'UNKNOWN'
        """)
        stats['unknown_indices'] = cursor.fetchone()[0]
        
        return stats
    
    def print_summary(self):
        """Print database summary"""
        stats = self.get_summary()
        
        print("\n" + "="*60)
        print("CAN MESSAGE DATABASE SUMMARY")
        print("="*60)
        print(f"Database: {self.db_path}")
        print(f"Total Messages: {stats['total_messages']:,}")
        print(f"First Message: {stats['first_message']}")
        print(f"Last Message: {stats['last_message']}")
        print(f"Unique Parameters: {stats['unique_parameters']}")
        print(f"Unknown Indices: {stats['unknown_indices']}")
        
        print("\nMessages by Addressee:")
        for row in stats['by_addressee']:
            print(f"  {row['addressee']:15} {row['count']:>10,} messages")
        print("="*60 + "\n")
    
    def get_recent_messages(self, hours=1, limit=50):
        """Get recent messages from the last N hours"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT timestamp, can_id, addressee, elster_name, 
                   interpreted_value, value_type, raw_bytes
            FROM can_messages
            WHERE timestamp > datetime('now', '-{} hours')
            ORDER BY timestamp DESC
            LIMIT {}
        """.format(hours, limit))
        return cursor.fetchall()
    
    def get_parameter_history(self, parameter_name, hours=24):
        """Get history for a specific parameter"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT timestamp, addressee, interpreted_value, raw_bytes
            FROM can_messages
            WHERE elster_name = ?
              AND timestamp > datetime('now', '-{} hours')
            ORDER BY timestamp DESC
        """.format(hours), (parameter_name,))
        return cursor.fetchall()
    
    def get_temperature_parameters(self):
        """Get all temperature-related parameters"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT DISTINCT elster_name
            FROM can_messages
            WHERE elster_name LIKE '%TEMP%'
            ORDER BY elster_name
        """)
        return [row[0] for row in cursor.fetchall()]
    
    def get_unknown_indices(self):
        """Get all unknown Elster indices"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT elster_index, raw_bytes, COUNT(*) as occurrences,
                   MIN(timestamp) as first_seen, MAX(timestamp) as last_seen
            FROM can_messages
            WHERE elster_name = 'INDEX_NOT_FOUND' OR elster_name LIKE 'UNKNOWN%'
            GROUP BY elster_index, raw_bytes
            ORDER BY occurrences DESC
        """)
        return cursor.fetchall()
    
    def export_to_csv(self, output_path, hours=24):
        """Export recent data to CSV"""
        if not PANDAS_AVAILABLE:
            print("Error: pandas is required for CSV export. Install with 'pip install pandas'")
            return False
        
        query = """
            SELECT timestamp, can_id, addressee, elster_index, elster_name,
                   interpreted_value, value_type, raw_bytes, raw_value
            FROM can_messages
            WHERE timestamp > datetime('now', '-{} hours')
            ORDER BY timestamp DESC
        """.format(hours)
        
        df = pd.read_sql_query(query, self.conn)
        df.to_csv(output_path, index=False)
        print(f"Exported {len(df)} messages to {output_path}")
        return True
    
    def analyze_message_frequency(self, minutes=60):
        """Analyze message frequency per minute"""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT 
                strftime('%Y-%m-%d %H:%M', timestamp) as minute,
                COUNT(*) as message_count
            FROM can_messages
            WHERE timestamp > datetime('now', '-{} minutes')
            GROUP BY minute
            ORDER BY minute DESC
        """.format(minutes))
        return cursor.fetchall()


def main():
    parser = argparse.ArgumentParser(
        description='Analyze CAN message SQLite database from ESP32',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s can_messages.db
  %(prog)s can_messages.db --recent 2
  %(prog)s can_messages.db --parameter OUTSIDE_TEMP --hours 48
  %(prog)s can_messages.db --unknown
  %(prog)s can_messages.db --export output.csv
        """
    )
    
    parser.add_argument('database', type=str, help='Path to SQLite database file')
    parser.add_argument('--recent', type=int, metavar='HOURS', 
                       help='Show recent messages from last N hours')
    parser.add_argument('--parameter', type=str, 
                       help='Show history for specific parameter')
    parser.add_argument('--hours', type=int, default=24,
                       help='Number of hours for parameter history (default: 24)')
    parser.add_argument('--unknown', action='store_true',
                       help='Show unknown Elster indices')
    parser.add_argument('--temps', action='store_true',
                       help='List all temperature parameters')
    parser.add_argument('--frequency', type=int, metavar='MINUTES',
                       help='Analyze message frequency per minute')
    parser.add_argument('--export', type=str, metavar='FILE',
                       help='Export to CSV file')
    
    args = parser.parse_args()
    
    # Check if database exists
    if not Path(args.database).exists():
        print(f"Error: Database file '{args.database}' not found")
        sys.exit(1)
    
    analyzer = CanDatabaseAnalyzer(args.database)
    
    # Always show summary
    analyzer.print_summary()
    
    # Handle specific requests
    if args.recent:
        print(f"\nRecent Messages (last {args.recent} hour(s)):")
        print("-" * 60)
        messages = analyzer.get_recent_messages(hours=args.recent, limit=50)
        for msg in messages:
            print(f"{msg['timestamp']} | {msg['addressee']:10} | {msg['elster_name']:30} | {msg['interpreted_value']}")
    
    if args.parameter:
        print(f"\nHistory for '{args.parameter}' (last {args.hours} hours):")
        print("-" * 60)
        history = analyzer.get_parameter_history(args.parameter, hours=args.hours)
        if history:
            for h in history[:50]:  # Limit to 50 entries
                print(f"{h['timestamp']} | {h['addressee']:10} | {h['interpreted_value']:15} | {h['raw_bytes']}")
        else:
            print(f"No messages found for parameter '{args.parameter}'")
    
    if args.unknown:
        print("\nUnknown Elster Indices:")
        print("-" * 80)
        unknown = analyzer.get_unknown_indices()
        if unknown:
            for u in unknown:
                print(f"Index: 0x{u['elster_index']:04X} | Occurrences: {u['occurrences']:>6} | "
                      f"Raw: {u['raw_bytes']:25} | First: {u['first_seen']}")
        else:
            print("No unknown indices found!")
    
    if args.temps:
        print("\nTemperature Parameters:")
        print("-" * 40)
        temps = analyzer.get_temperature_parameters()
        for temp in temps:
            print(f"  {temp}")
    
    if args.frequency:
        print(f"\nMessage Frequency (per minute, last {args.frequency} minutes):")
        print("-" * 40)
        freq = analyzer.analyze_message_frequency(minutes=args.frequency)
        for f in freq:
            print(f"{f['minute']} | {f['message_count']:>5} messages")
    
    if args.export:
        print(f"\nExporting to {args.export}...")
        analyzer.export_to_csv(args.export, hours=args.hours)


if __name__ == '__main__':
    main()



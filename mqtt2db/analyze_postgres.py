#!/usr/bin/env python3
"""
Quick PostgreSQL/MySQL Database Analysis for CAN Messages

Usage:
    python3 analyze_postgres.py
    
Requirements:
    sudo apt install python3-pandas python3-sqlalchemy python3-psycopg2
"""

import os
import sys
from datetime import datetime, timedelta

# Silence SQLAlchemy warnings
os.environ['SQLALCHEMY_SILENCE_UBER_WARNING'] = '1'

try:
    import pandas as pd
    from sqlalchemy import create_engine
except ImportError as e:
    print(f"Missing required package: {e}")
    print("Install with: sudo apt install python3-pandas python3-sqlalchemy python3-psycopg2")
    sys.exit(1)

# Database connection string
DB_URL = "postgresql://stiebel:your_password_here@localhost/can_messages"

def get_summary(engine):
    """Get database summary"""
    print("\n" + "="*60)
    print("CAN MESSAGE DATABASE SUMMARY")
    print("="*60)
    
    # Total messages
    total = pd.read_sql_query("SELECT COUNT(*) as count FROM can_messages", engine)
    print(f"Total Messages: {total['count'][0]:,}")
    
    # Date range
    dates = pd.read_sql_query("""
        SELECT MIN(timestamp) as first, MAX(timestamp) as last 
        FROM can_messages
    """, engine)
    print(f"First Message: {dates['first'][0]}")
    print(f"Last Message: {dates['last'][0]}")
    
    # Unique parameters
    params = pd.read_sql_query("SELECT COUNT(DISTINCT name) as count FROM can_messages", engine)
    print(f"Unique Parameters: {params['count'][0]}")
    
    # By addressee
    print("\nMessages by CAN Device:")
    by_device = pd.read_sql_query("""
        SELECT sender_can_id, sender_name, COUNT(*) as count 
        FROM can_messages 
        GROUP BY sender_can_id, sender_name 
        ORDER BY count DESC
    """, engine)
    for _, row in by_device.iterrows():
        print(f"  0x{row['sender_can_id']:03X} ({row['sender_name']:15}) {row['count']:>10,} messages")
    
    print("="*60)

def get_recent_messages(engine, hours=2):
    """Get recent messages"""
    print(f"\nRecent Messages (last {hours} hours):")
    print("-"*80)
    
    query = f"""
        SELECT timestamp, sender_can_id, sender_name, name, value, value_type
        FROM can_messages
        WHERE timestamp > NOW() - INTERVAL '{hours} hours'
        ORDER BY timestamp DESC
        LIMIT 50
    """
    df = pd.read_sql_query(query, engine)
    print(df.to_string(index=False))

def get_parameter_history(engine, param_name, hours=24):
    """Get history for specific parameter"""
    print(f"\nHistory for '{param_name}' (last {hours} hours):")
    print("-"*80)
    
    query = f"""
        SELECT timestamp, sender_can_id, sender_name, value, raw_value
        FROM can_messages
        WHERE name = '{param_name}'
          AND timestamp > NOW() - INTERVAL '{hours} hours'
        ORDER BY timestamp DESC
        LIMIT 100
    """
    df = pd.read_sql_query(query, engine)
    print(df.to_string(index=False))

def get_all_parameters(engine):
    """List all unique parameters"""
    print("\nAll Parameters:")
    print("-"*40)
    
    query = """
        SELECT DISTINCT name, COUNT(*) as count
        FROM can_messages
        GROUP BY name
        ORDER BY name
    """
    df = pd.read_sql_query(query, engine)
    print(df.to_string(index=False))

def export_to_csv(engine, output_file, hours=24):
    """Export to CSV"""
    query = f"""
        SELECT timestamp, sender_can_id, sender_name, elster_idx, name, value, value_type, raw_hex, raw_value
        FROM can_messages
        WHERE timestamp > NOW() - INTERVAL '{hours} hours'
        ORDER BY timestamp DESC
    """
    df = pd.read_sql_query(query, engine)
    df.to_csv(output_file, index=False)
    print(f"\nExported {len(df)} messages to {output_file}")

def get_can_ids(engine):
    """Show actual CAN IDs being used"""
    print("\nActual CAN IDs in Database:")
    print("-"*60)
    
    query = """
        SELECT sender_can_id, 
               '0x' || LPAD(TO_HEX(sender_can_id), 3, '0') as hex_id,
               COUNT(*) as count,
               COUNT(DISTINCT name) as unique_params
        FROM can_messages
        GROUP BY sender_can_id
        ORDER BY count DESC
    """
    df = pd.read_sql_query(query, engine)
    print(df.to_string(index=False))

def main():
    print(f"Connecting to: {DB_URL.split('@')[1]}")  # Don't show password
    
    try:
        engine = create_engine(DB_URL)
        
        # Run analyses
        get_summary(engine)
        get_can_ids(engine)
        get_recent_messages(engine, hours=2)
        
        # Uncomment to use:
        # get_parameter_history(engine, "OUTSIDE_TEMP", hours=48)
        # get_all_parameters(engine)
        # export_to_csv(engine, "can_data.csv", hours=24)
        
    except Exception as e:
        print(f"\nError: {e}")
        print("\nMake sure:")
        print("1. PostgreSQL is running")
        print("2. Database credentials are correct in DB_URL")
        print("3. Database 'can_messages' exists")
        sys.exit(1)

if __name__ == '__main__':
    main()


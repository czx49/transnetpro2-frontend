import pandas as pd
import os
import sys

# 设置控制台输出编码
sys.stdout.reconfigure(encoding='utf-8')

# 读取Excel文件
excel_path = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.xlsx"
try:
    print(f"尝试读取文件: {excel_path}")
    print(f"文件是否存在: {os.path.exists(excel_path)}")

    df = pd.read_excel(excel_path)

    # 打印列名
    print("\n列名:", list(df.columns))

    # 打印数据形状
    print(f"\n数据形状: {df.shape}")

    # 打印前10行数据
    print("\n前10行数据:")
    print(df.head(10).to_string())

except Exception as e:
    print(f"读取Excel文件时出错: {e}")

import pandas as pd
import json

# 读取Excel文件
excel_path = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.xlsx"
try:
    df = pd.read_excel(excel_path)
    # 将数据转换为JSON格式并打印前20行
    result = df.head(20).to_json(orient="records", force_ascii=False)
    print(json.dumps(json.loads(result), ensure_ascii=False, indent=2))
    # 打印列名
    print("\n列名:", list(df.columns))
    # 打印数据形状
    print(f"\n数据形状: {df.shape}")
except Exception as e:
    print(f"读取Excel文件时出错: {e}")

import pandas as pd
import sys

csv_path = r"d:\Projects-swjtu-phd\OD_estimate_git\OD_Estimate\exports\user_project.project59_user1_road_way.csv"
output_path = r"d:\Projects-swjtu-phd\OD_estimate_git\OD_Estimate\bin\x64\csv_columns.txt"

try:
    df = pd.read_csv(csv_path)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(f"Columns: {df.columns.tolist()}\n")
        f.write(str(df.head(3)))
    print("Done")
except Exception as e:
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(f"Error: {e}")
    print("Error")

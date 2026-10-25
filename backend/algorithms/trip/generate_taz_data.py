import pandas as pd
import os
import sys

# 创建输出目录
output_dir = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\trip_generation\data"
os.makedirs(output_dir, exist_ok=True)

# 读取交通小区数据
taz_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.csv"
taz_df = pd.read_csv(taz_file)

print(f"读取了 {len(taz_df)} 条交通小区数据")

# 创建结果数据框
result_df = pd.DataFrame()
result_df['id'] = taz_df['area_id']

# 使用交通生成量和吸引量作为基础数据
# 设置转换系数
pop_factor = 0.05    # 人口系数
cars_factor = 0.02   # 车辆系数
eind_factor = 0.01   # 工业就业系数
eret_factor = 0.015  # 零售就业系数
eoth_factor = 0.02   # 其他就业系数
du_factor = 0.3      # 住宅单元系数

# 创建所需字段
result_df['pop'] = taz_df['traffic_generation_volume'] * pop_factor
result_df['cars'] = taz_df['traffic_generation_volume'] * cars_factor
result_df['eind'] = taz_df['traffic_attraction_volume'] * eind_factor
result_df['eret'] = taz_df['traffic_attraction_volume'] * eret_factor
result_df['eoth'] = taz_df['traffic_attraction_volume'] * eoth_factor
result_df['du'] = result_df['pop'] * du_factor
result_df['hinc'] = 50000 + result_df['cars'] * 5000
result_df['area'] = 1000  # 假设每个小区1000平方米
result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']

# 保存结果
output_file = os.path.join(output_dir, 'other_taz_socioeconomic.csv')
result_df.to_csv(output_file, index=False)

print(f"结果已保存到: {output_file}")
print(f"数据形状: {result_df.shape}")
print("\n数据前5行:")
print(result_df.head(5))

# 生成配置文件示例
config_file = os.path.join(output_dir, 'config.json')
config_content = '''{
  "db_conn_str": {
    "host": "localhost",
    "port": 5432,
    "dbname": "trip_generation_db",
    "user": "postgres",
    "password": "<CONFIGURE_LOCALLY>",
    "role": "user_project",
    "scenario_prefix": "test"
  },
  "algorithm_params": {
    "production_rates": {
      "pop": 2.0,
      "cars": 0.7,
      "eind": 0.1,
      "eret": 0.2,
      "eoth": 0.15,
      "du": 0.1,
      "hinc": 0.000005,
      "area": 0.0,
      "etot": 0.0
    },
    "attraction_rates": {
      "pop": 0.5,
      "cars": 0.1,
      "eind": 1.5,
      "eret": 3.0,
      "eoth": 1.5,
      "du": 0.1,
      "hinc": 0.000005,
      "area": 0.1,
      "etot": 0.0
    }
  }
}'''

with open(config_file, 'w', encoding='utf-8') as f:
    f.write(config_content)

print(f"配置文件示例已保存到: {config_file}")

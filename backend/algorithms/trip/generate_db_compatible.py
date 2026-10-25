import pandas as pd
import os
import datetime
import struct
import binascii

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

# 创建所需字段 - 注意转换为整数类型
result_df['pop'] = (taz_df['traffic_generation_volume'] * pop_factor).astype(int)
result_df['cars'] = (taz_df['traffic_generation_volume'] * cars_factor).astype(int)
result_df['eind'] = (taz_df['traffic_attraction_volume'] * eind_factor).astype(int)
result_df['eret'] = (taz_df['traffic_attraction_volume'] * eret_factor).astype(int)
result_df['eoth'] = (taz_df['traffic_attraction_volume'] * eoth_factor).astype(int)
result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']  # 注意etot位置调整
result_df['du'] = (result_df['pop'] * du_factor).astype(int)
result_df['hinc'] = 50000 + result_df['cars'] * 5000
result_df['area'] = 1000  # 假设每个小区1000平方米

# 简单设置经纬度
result_df['longitude'] = 104.0 + result_df.index * 0.01
result_df['latitude'] = 30.0 + result_df.index * 0.01

# 生成WKB格式的Point几何数据
def create_point_wkb(lon, lat):
    # WKB格式: 0101000020E6100000 + 8字节经度 + 8字节纬度
    # 0101000020E6100000 是 SRID 4326 的 Point 类型的WKB头
    lon_hex = struct.pack('<d', lon).hex()
    lat_hex = struct.pack('<d', lat).hex()
    return f"0101000020E6100000{lon_hex}{lat_hex}"

result_df['geometry'] = result_df.apply(
    lambda row: create_point_wkb(row['longitude'], row['latitude']),
    axis=1
)

# 添加created_at字段
current_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
result_df['created_at'] = current_time

# 保存结果 - 使用双引号包围字段名
output_file = os.path.join(output_dir, 'other_taz_socioeconomic_db.csv')
result_df.to_csv(output_file, index=False, quoting=1)  # quoting=1表示使用双引号包围所有字段

print(f"结果已保存到: {output_file}")
print(f"数据形状: {result_df.shape}")
print("\n数据前5行:")
print(result_df.head(5))

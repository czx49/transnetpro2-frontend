import pandas as pd
import os
import datetime
import re

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
result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']  # 注意etot位置调整
result_df['du'] = result_df['pop'] * du_factor
result_df['hinc'] = 50000 + result_df['cars'] * 5000
result_df['area'] = 1000  # 假设每个小区1000平方米

# 从geometry字段中提取中心点坐标
# 这里使用一个简单的方法：从WKB格式中提取第一个坐标点作为中心点
def extract_centroid(geometry_str):
    # 提取所有坐标点
    coords = re.findall(r'([0-9.]+)5B40([0-9.]+)3240', geometry_str)
    if coords:
        # 计算中心点
        x_sum = sum(float(x) for x, _ in coords)
        y_sum = sum(float(y) for _, y in coords)
        x_avg = x_sum / len(coords)
        y_avg = y_sum / len(coords)
        # 返回Point格式的WKB
        return f"0101000020E6100000{x_avg:016X}{y_avg:016X}"
    else:
        # 默认点
        return "0101000020E61000000000000000005E400000000000003E40"  # 默认点 (120, 30)

# 提取经纬度和生成Point几何
longitudes = []
latitudes = []
point_geometries = []

for idx, row in taz_df.iterrows():
    # 提取中心点
    coords = re.findall(r'([0-9.]+)5B40([0-9.]+)3240', row['geometry'])
    if coords:
        # 计算中心点
        x_sum = sum(float(x) for x, _ in coords)
        y_sum = sum(float(y) for _, y in coords)
        x_avg = x_sum / len(coords)
        y_avg = y_sum / len(coords)

        # 存储经纬度
        longitudes.append(x_avg)
        latitudes.append(y_avg)

        # 创建Point几何
        point_wkb = f"0101000020E6100000000000000000{x_avg}40000000000000{y_avg}40"
    else:
        # 默认值
        longitudes.append(104.0 + idx * 0.01)
        latitudes.append(30.0 + idx * 0.01)
        point_wkb = f"0101000020E61000000000000000005E400000000000003E40"  # 默认点 (120, 30)

    point_geometries.append(point_wkb)

# 添加到结果数据框
result_df['longitude'] = longitudes
result_df['latitude'] = latitudes
result_df['geometry'] = "POINT(104 30)"  # 使用简单的WKT格式表示点

# 添加created_at字段
current_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
result_df['created_at'] = current_time

# 保存结果 - 使用双引号包围字段名
output_file = os.path.join(output_dir, 'other_taz_socioeconomic_point.csv')
result_df.to_csv(output_file, index=False, quoting=1)  # quoting=1表示使用双引号包围所有字段

print(f"结果已保存到: {output_file}")
print(f"数据形状: {result_df.shape}")
print("\n数据前5行:")
print(result_df.head(5))

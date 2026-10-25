import pandas as pd
import os

# 创建输出目录
output_dir = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\trip_generation\data"
os.makedirs(output_dir, exist_ok=True)

# 直接创建所需的数据结构
# 从交通小区数据中获取ID
taz_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.csv"
taz_df = pd.read_csv(taz_file)

# 从POI统计数据中获取相关信息
poi_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\小区poi统计.csv"
try:
    poi_df = pd.read_csv(poi_file, encoding='gbk')
except:
    try:
        poi_df = pd.read_csv(poi_file, encoding='utf-8')
    except:
        poi_df = pd.read_csv(poi_file, encoding='cp936')

# 创建结果数据框
result_df = pd.DataFrame()
result_df['id'] = taz_df['area_id']

# 基于交通小区数据中的traffic_generation_volume和traffic_attraction_volume设置基础数据
# 这些数据可以作为人口和车辆数的基础
result_df['traffic_gen'] = taz_df['traffic_generation_volume']
result_df['traffic_attr'] = taz_df['traffic_attraction_volume']

# 创建所需字段
# 人口 (pop): 使用交通生成量作为基础
result_df['pop'] = result_df['traffic_gen'] * 0.05  # 交通生成量的5%作为人口数

# 车辆 (cars): 使用交通生成量的一部分
result_df['cars'] = result_df['traffic_gen'] * 0.02  # 交通生成量的2%作为车辆数

# 工业就业 (eind): 基于交通吸引量
result_df['eind'] = result_df['traffic_attr'] * 0.01  # 交通吸引量的1%作为工业就业

# 零售就业 (eret): 基于交通吸引量
result_df['eret'] = result_df['traffic_attr'] * 0.015  # 交通吸引量的1.5%作为零售就业

# 其他就业 (eoth): 基于交通吸引量
result_df['eoth'] = result_df['traffic_attr'] * 0.02  # 交通吸引量的2%作为其他就业

# 住宅单元 (du): 基于人口数
result_df['du'] = result_df['pop'] * 0.3  # 人口的30%作为住宅单元数

# 家庭收入 (hinc): 设置一个基础值
result_df['hinc'] = 50000 + result_df['cars'] * 5000  # 基础收入+车辆数*系数

# 面积 (area): 设置一个基础值
result_df['area'] = 1000  # 假设每个小区1000平方米

# 总就业 (etot): 工业+零售+其他就业总和
result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']

# 只保留需要的列
final_df = result_df[['id', 'pop', 'cars', 'eind', 'eret', 'eoth', 'du', 'hinc', 'area', 'etot']]

# 保存结果
output_file = os.path.join(output_dir, 'other_taz_socioeconomic.csv')
final_df.to_csv(output_file, index=False)

print(f"结果已保存到: {output_file}")
print(f"数据形状: {final_df.shape}")
print("\n数据前10行:")
print(final_df.head(10))

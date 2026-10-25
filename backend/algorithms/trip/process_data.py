import pandas as pd
import numpy as np
import os

# 读取小区POI统计数据
poi_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\小区poi统计.csv"
poi_df = pd.read_csv(poi_file, encoding='gbk')  # 使用gbk编码读取中文

# 读取交通小区数据
taz_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.csv"
taz_df = pd.read_csv(taz_file)

# 显示数据基本信息
print("POI数据列名:", poi_df.columns.tolist())
print("POI数据形状:", poi_df.shape)
print("交通小区数据列名:", taz_df.columns.tolist())
print("交通小区数据形状:", taz_df.shape)

# 查看前几行数据
print("\nPOI数据前5行:")
print(poi_df.head())
print("\n交通小区数据前5行:")
print(taz_df.head())

# 创建other_taz_socioeconomic数据格式
# 根据trip_generation.py中的DEFAULT_ATTR_RATES和DEFAULT_PROD_RATES需要的字段:
# pop, cars, eind, eret, eoth, du, hinc, area, etot

# 将两个数据集合并，使用fid作为连接键
# 假设poi_df中的fid对应taz_df中的area_id
poi_df['fid'] = poi_df['fid'].astype(int)
taz_df['area_id'] = taz_df['area_id'].astype(int)

# 合并数据
merged_df = pd.merge(taz_df, poi_df, left_on='area_id', right_on='fid', how='left')
print("\n合并后数据形状:", merged_df.shape)
print("\n合并后数据前5行:")
print(merged_df.head())

# 创建所需字段
# 定义映射规则 - 这里根据POI数据特点定义合理的映射
result_df = pd.DataFrame()
result_df['id'] = merged_df['area_id']  # 使用area_id作为id

# 人口 (pop): 使用车辆数作为基础，乘以一个系数
result_df['pop'] = merged_df['车辆数'] * 3  # 假设每辆车对应3个人口

# 车辆 (cars): 直接使用车辆数
result_df['cars'] = merged_df['车辆数']

# 工业就业 (eind): 根据工业相关POI估算
result_df['eind'] = merged_df['政府办公'] * 10 + merged_df['公共设施'] * 5

# 零售就业 (eret): 根据商业相关POI估算
result_df['eret'] = merged_df['购物'] * 5 + merged_df['餐饮'] * 3

# 其他就业 (eoth): 根据服务相关POI估算
result_df['eoth'] = merged_df['生活服务'] * 3 + merged_df['金融'] * 5 + merged_df['医疗'] * 8

# 住宅单元 (du): 估算住宅单元数量
result_df['du'] = merged_df['车辆数'] * 0.8  # 假设80%的车对应一个住宅单元

# 家庭收入 (hinc): 估算平均家庭收入
result_df['hinc'] = 50000 + merged_df['车辆数'] * 1000  # 基础收入+车辆数*系数

# 面积 (area): 可以从几何数据估算，这里简化处理
result_df['area'] = 1000  # 假设每个小区1000平方米

# 总就业 (etot): 工业+零售+其他就业总和
result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']

# 处理缺失值
result_df = result_df.fillna(0)

# 保存结果
output_dir = os.path.join(os.path.dirname(os.path.dirname(poi_file)), 'trip_generation', 'data')
os.makedirs(output_dir, exist_ok=True)
output_file = os.path.join(output_dir, 'other_taz_socioeconomic.csv')
result_df.to_csv(output_file, index=False)

print(f"\n结果已保存到: {output_file}")
print("\n结果数据前10行:")
print(result_df.head(10))

# 显示统计信息
print("\n结果数据统计:")
print(result_df.describe())

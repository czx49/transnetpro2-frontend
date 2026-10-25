import pandas as pd
import os
import numpy as np

def main():
    # 创建输出目录
    output_dir = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\trip_generation\data"
    os.makedirs(output_dir, exist_ok=True)

    # 读取交通小区数据
    taz_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\project59_user1_road_community_export_2025-12-11T03-46-31-589Z_1mwp.csv"
    taz_df = pd.read_csv(taz_file)

    # 尝试不同编码读取POI统计数据
    poi_file = r"d:\Projects-swjtu-phd\tnpapp1221\bin\algorithm\trip\小区poi统计.csv"
    encodings = ['gbk', 'utf-8', 'cp936', 'gb18030']

    for encoding in encodings:
        try:
            poi_df = pd.read_csv(poi_file, encoding=encoding)
            print(f"成功使用{encoding}编码读取POI数据")
            break
        except Exception as e:
            print(f"使用{encoding}编码读取失败: {e}")
    else:
        print("无法读取POI数据，将只使用交通小区数据")
        poi_df = None

    # 创建结果数据框
    result_df = pd.DataFrame()
    result_df['id'] = taz_df['area_id']

    # 基于交通小区数据设置基础数据
    # 定义系数 - 这些系数可以根据实际情况调整
    coefficients = {
        'pop_from_gen': 0.05,     # 交通生成量转人口系数
        'cars_from_gen': 0.02,    # 交通生成量转车辆系数
        'eind_from_attr': 0.01,   # 交通吸引量转工业就业系数
        'eret_from_attr': 0.015,  # 交通吸引量转零售就业系数
        'eoth_from_attr': 0.02,   # 交通吸引量转其他就业系数
        'du_from_pop': 0.3,       # 人口转住宅单元系数
        'hinc_base': 50000,       # 基础家庭收入
        'hinc_per_car': 5000,     # 每辆车增加的家庭收入
        'area_base': 1000         # 基础面积
    }

    # 创建所需字段
    # 人口 (pop)
    result_df['pop'] = taz_df['traffic_generation_volume'] * coefficients['pop_from_gen']

    # 车辆 (cars)
    result_df['cars'] = taz_df['traffic_generation_volume'] * coefficients['cars_from_gen']

    # 工业就业 (eind)
    result_df['eind'] = taz_df['traffic_attraction_volume'] * coefficients['eind_from_attr']

    # 零售就业 (eret)
    result_df['eret'] = taz_df['traffic_attraction_volume'] * coefficients['eret_from_attr']

    # 其他就业 (eoth)
    result_df['eoth'] = taz_df['traffic_attraction_volume'] * coefficients['eoth_from_attr']

    # 住宅单元 (du)
    result_df['du'] = result_df['pop'] * coefficients['du_from_pop']

    # 家庭收入 (hinc)
    result_df['hinc'] = coefficients['hinc_base'] + result_df['cars'] * coefficients['hinc_per_car']

    # 面积 (area)
    result_df['area'] = coefficients['area_base']

    # 总就业 (etot)
    result_df['etot'] = result_df['eind'] + result_df['eret'] + result_df['eoth']

    # 如果有POI数据，可以用它来调整一些值
    if poi_df is not None:
        try:
            # 将fid转换为整数以便匹配
            poi_df['fid'] = poi_df['fid'].astype(float).astype(int)

            # 创建映射字典
            poi_data = {}
            for _, row in poi_df.iterrows():
                fid = row['fid']
                poi_data[fid] = {
                    'vehicles': row['车辆数'] if '车辆数' in poi_df.columns else 0,
                    'dining': row['餐饮'] if '餐饮' in poi_df.columns else 0,
                    'public_facilities': row['公共设施'] if '公共设施' in poi_df.columns else 0,
                    'shopping': row['购物'] if '购物' in poi_df.columns else 0,
                    'transport': row['交通设施'] if '交通设施' in poi_df.columns else 0,
                    'education': row['教育'] if '教育' in poi_df.columns else 0,
                    'finance': row['金融'] if '金融' in poi_df.columns else 0,
                    'attractions': row['景点'] if '景点' in poi_df.columns else 0,
                    'business_residential': row['商务住宅'] if '商务住宅' in poi_df.columns else 0,
                    'life_services': row['生活服务'] if '生活服务' in poi_df.columns else 0,
                    'sports': row['体育'] if '体育' in poi_df.columns else 0,
                    'medical': row['医疗'] if '医疗' in poi_df.columns else 0,
                    'government': row['政府办公'] if '政府办公' in poi_df.columns else 0,
                    'accommodation': row['住宿服务'] if '住宿服务' in poi_df.columns else 0
                }

            # 使用POI数据调整结果
            for i, row in result_df.iterrows():
                taz_id = row['id']
                if taz_id in poi_data:
                    # 调整车辆数
                    if poi_data[taz_id]['vehicles'] > 0:
                        result_df.at[i, 'cars'] = poi_data[taz_id]['vehicles']

                    # 调整人口 - 基于车辆数
                    result_df.at[i, 'pop'] = max(result_df.at[i, 'pop'], poi_data[taz_id]['vehicles'] * 3)

                    # 调整工业就业 - 基于政府办公和公共设施
                    result_df.at[i, 'eind'] = max(
                        result_df.at[i, 'eind'],
                        poi_data[taz_id]['government'] * 10 + poi_data[taz_id]['public_facilities'] * 5
                    )

                    # 调整零售就业 - 基于购物和餐饮
                    result_df.at[i, 'eret'] = max(
                        result_df.at[i, 'eret'],
                        poi_data[taz_id]['shopping'] * 5 + poi_data[taz_id]['dining'] * 3
                    )

                    # 调整其他就业 - 基于服务类POI
                    result_df.at[i, 'eoth'] = max(
                        result_df.at[i, 'eoth'],
                        poi_data[taz_id]['life_services'] * 3 +
                        poi_data[taz_id]['finance'] * 5 +
                        poi_data[taz_id]['medical'] * 8
                    )

                    # 重新计算总就业
                    result_df.at[i, 'etot'] = result_df.at[i, 'eind'] + result_df.at[i, 'eret'] + result_df.at[i, 'eoth']

            print("已使用POI数据调整结果")
        except Exception as e:
            print(f"使用POI数据调整结果时出错: {e}")

    # 确保所有值都是非负的
    for col in result_df.columns:
        if col != 'id':
            result_df[col] = result_df[col].apply(lambda x: max(0, x))

    # 保存结果
    output_file = os.path.join(output_dir, 'other_taz_socioeconomic.csv')
    result_df.to_csv(output_file, index=False)

    print(f"结果已保存到: {output_file}")
    print(f"数据形状: {result_df.shape}")
    print("\n数据前10行:")
    print(result_df.head(10))

    # 显示统计信息
    print("\n数据统计:")
    print(result_df.describe())

if __name__ == "__main__":
    main()

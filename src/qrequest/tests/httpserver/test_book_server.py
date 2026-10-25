#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
图书管理服务器测试客户端
用于验证book_server.py的各种API功能
"""

import json
import requests
import time
from datetime import datetime


class BookServerTester:
    """图书服务器测试类"""
    
    def __init__(self, base_url="http://localhost:8888"):
        self.base_url = base_url
        self.session = requests.Session()
        self.session.headers.update({
            'Content-Type': 'application/json',
            'User-Agent': 'BookServerTester/1.0'
        })
    
    def print_response(self, response, operation):
        """打印响应信息"""
        print(f"\n{'='*60}")
        print(f"操作: {operation}")
        print(f"状态码: {response.status_code}")
        print(f"响应时间: {response.elapsed.total_seconds():.3f}s")
        
        try:
            data = response.json()
            print(f"响应内容:")
            print(json.dumps(data, ensure_ascii=False, indent=2))
        except json.JSONDecodeError:
            print(f"响应内容: {response.text}")
    
    def test_health_check(self):
        """测试健康检查"""
        try:
            response = self.session.get(f"{self.base_url}/health")
            self.print_response(response, "健康检查")
            return response.status_code == 200
        except Exception as e:
            print(f"健康检查失败: {e}")
            return False
    
    def test_get_all_books(self):
        """测试获取所有图书"""
        try:
            response = self.session.get(f"{self.base_url}/books")
            self.print_response(response, "获取所有图书")
            
            if response.status_code == 200:
                data = response.json()
                books = data.get('data', [])
                print(f"共找到 {len(books)} 本图书")
                return books
            return []
        except Exception as e:
            print(f"获取图书列表失败: {e}")
            return []
    
    def test_search_books(self, keyword="Python"):
        """测试搜索图书"""
        try:
            response = self.session.get(f"{self.base_url}/books?search={keyword}")
            self.print_response(response, f"搜索图书 (关键词: {keyword})")
            
            if response.status_code == 200:
                data = response.json()
                books = data.get('data', [])
                print(f"搜索到 {len(books)} 本相关图书")
                return books
            return []
        except Exception as e:
            print(f"搜索图书失败: {e}")
            return []
    
    def test_get_book(self, book_id):
        """测试获取指定图书"""
        try:
            response = self.session.get(f"{self.base_url}/books/{book_id}")
            self.print_response(response, f"获取图书 (ID: {book_id})")
            
            if response.status_code == 200:
                return response.json().get('data')
            return None
        except Exception as e:
            print(f"获取指定图书失败: {e}")
            return None
    
    def test_create_book(self):
        """测试创建图书"""
        new_book = {
            "title": f"测试图书_{datetime.now().strftime('%H%M%S')}",
            "author": "测试作者",
            "isbn": f"978-0-{int(time.time()) % 1000000:06d}-{datetime.now().microsecond % 100:02d}-0",
            "publish_date": "2024-01-01"
        }
        
        try:
            response = self.session.post(
                f"{self.base_url}/books",
                data=json.dumps(new_book, ensure_ascii=False),
                headers={'Content-Type': 'application/json'}
            )
            self.print_response(response, "创建新图书")
            
            if response.status_code == 201:
                return response.json().get('data')
            return None
        except Exception as e:
            print(f"创建图书失败: {e}")
            return None
    
    def test_update_book(self, book_id):
        """测试更新图书"""
        update_data = {
            "title": f"更新后的标题_{datetime.now().strftime('%H%M%S')}",
            "author": "更新后的作者"
        }
        
        try:
            response = self.session.put(
                f"{self.base_url}/books/{book_id}",
                data=json.dumps(update_data, ensure_ascii=False),
                headers={'Content-Type': 'application/json'}
            )
            self.print_response(response, f"更新图书 (ID: {book_id})")
            
            if response.status_code == 200:
                return response.json().get('data')
            return None
        except Exception as e:
            print(f"更新图书失败: {e}")
            return None
    
    def test_delete_book(self, book_id):
        """测试删除图书"""
        try:
            response = self.session.delete(f"{self.base_url}/books/{book_id}")
            self.print_response(response, f"删除图书 (ID: {book_id})")
            return response.status_code == 200
        except Exception as e:
            print(f"删除图书失败: {e}")
            return False
    
    def test_invalid_requests(self):
        """测试无效请求"""
        print(f"\n{'='*60}")
        print("测试无效请求处理")
        
        # 测试不存在的端点
        response = self.session.get(f"{self.base_url}/invalid")
        self.print_response(response, "访问不存在的端点")
        
        # 测试无效的图书ID
        response = self.session.get(f"{self.base_url}/books/invalid-id")
        self.print_response(response, "访问不存在的图书")
        
        # 测试无效的JSON数据
        response = self.session.post(
            f"{self.base_url}/books",
            data="invalid json",
            headers={'Content-Type': 'application/json'}
        )
        self.print_response(response, "发送无效JSON数据")
        
        # 测试缺少必需字段
        invalid_book = {"title": "仅有标题"}
        response = self.session.post(
            f"{self.base_url}/books",
            data=json.dumps(invalid_book),
            headers={'Content-Type': 'application/json'}
        )
        self.print_response(response, "创建缺少必需字段的图书")
    
    def run_full_test(self):
        """运行完整测试套件"""
        print("╔══════════════════════════════════════════════════════════════╗")
        print("║                    图书服务器API测试                           ║")
        print("║                       全功能测试                              ║")
        print("╚══════════════════════════════════════════════════════════════╝")
        
        # 1. 健康检查
        print("\n1. 健康检查...")
        if not self.test_health_check():
            print("❌ 服务器不可用，请先启动 book_server.py")
            return False
        print("✅ 服务器健康检查通过")
        
        # 2. 获取所有图书
        print("\n2. 获取所有图书...")
        books = self.test_get_all_books()
        print(f"✅ 成功获取 {len(books)} 本图书")
        
        # 3. 搜索图书
        print("\n3. 搜索图书...")
        search_results = self.test_search_books("Python")
        print(f"✅ 搜索结果: {len(search_results)} 本图书")
        
        # 4. 获取指定图书
        if books:
            print("\n4. 获取指定图书...")
            first_book = books[0]
            book_detail = self.test_get_book(first_book['id'])
            if book_detail:
                print("✅ 成功获取指定图书详情")
            else:
                print("❌ 获取指定图书失败")
        
        # 5. 创建新图书
        print("\n5. 创建新图书...")
        new_book = self.test_create_book()
        if new_book:
            print("✅ 成功创建新图书")
            created_book_id = new_book['id']
            
            # 6. 更新图书
            print("\n6. 更新图书...")
            updated_book = self.test_update_book(created_book_id)
            if updated_book:
                print("✅ 成功更新图书")
            else:
                print("❌ 更新图书失败")
            
            # 7. 删除图书
            print("\n7. 删除图书...")
            if self.test_delete_book(created_book_id):
                print("✅ 成功删除图书")
            else:
                print("❌ 删除图书失败")
        else:
            print("❌ 创建新图书失败，跳过更新和删除测试")
        
        # 8. 测试错误处理
        print("\n8. 测试错误处理...")
        self.test_invalid_requests()
        print("✅ 错误处理测试完成")
        
        print(f"\n{'='*60}")
        print("🎉 所有测试完成！")
        print("📝 请查看上面的详细测试结果")
        print(f"{'='*60}")
        
        return True


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='图书管理服务器测试客户端')
    parser.add_argument('--url', default='http://localhost:8888', 
                        help='服务器地址 (默认: http://localhost:8888)')
    parser.add_argument('--quick', action='store_true', 
                        help='仅运行快速健康检查')
    
    args = parser.parse_args()
    
    # 创建测试器
    tester = BookServerTester(args.url)
    
    if args.quick:
        # 快速测试
        print("快速健康检查...")
        if tester.test_health_check():
            print("✅ 服务器运行正常")
        else:
            print("❌ 服务器不可用")
    else:
        # 完整测试
        tester.run_full_test()


if __name__ == '__main__':
    main()
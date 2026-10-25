/*
 * Copyright (C) 2016 Alexander Makarov
 *
 * Source:
 * https://wiki.qt.io/Qt_thread-safe_singleton
 *
 * This file is a part of Breakpad-qt library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */


/**
 * @file singleton.h
 * @brief 通用线程安全单例模板，适用于Qt环境
 * @details 基于qCallOnce实现，支持懒加载、线程安全、自动析构，适合全局唯一对象管理。
 * 来源：https://wiki.qt.io/Qt_thread-safe_singleton
 */
#ifndef SINGLETON
#define SINGLETON

#include <QtGlobal>
#include <QScopedPointer>
#include "call_once.h"


/**
 * @brief 通用线程安全单例模板类
 * @tparam T 需要实现单例的类型
 * @note 通过qCallOnce保证多线程下只初始化一次，自动析构，禁止拷贝
 */
template <class T>
class Singleton
{
public:
    /**
     * @brief 获取单例实例（线程安全，懒加载）
     * @return T& 单例对象引用
     */
    static T& instance()
    {
        qCallOnce(init, flag); // 保证init只被调用一次
        return *tptr;
    }

    /**
     * @brief 单例对象初始化函数，仅首次调用时执行
     */
    static void init()
    {
        tptr.reset(new T);
    }

private:
    Singleton() {}                 //!< 禁止外部构造
    ~Singleton() {}                //!< 禁止外部析构
    Q_DISABLE_COPY(Singleton)      //!< 禁止拷贝和赋值

    static QScopedPointer<T> tptr; //!< 单例对象指针，自动析构
    static QBasicAtomicInt flag;   //!< 原子标志位，配合qCallOnce实现线程安全
};


// 静态成员初始化
template<class T> QScopedPointer<T> Singleton<T>::tptr(0);
template<class T> QBasicAtomicInt Singleton<T>::flag = Q_BASIC_ATOMIC_INITIALIZER(CallOnce::CO_Request);

#endif // SINGLETON


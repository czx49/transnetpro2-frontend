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
 * @file call_once.h
 * @brief 跨平台线程安全单次调用工具，适用于Qt环境下的单例/延迟初始化等场景
 * @details 支持全局/线程级别的只执行一次操作，兼容多版本Qt，适合多线程环境下的资源安全初始化。
 * 来源：https://wiki.qt.io/Qt_thread-safe_singleton
 */
#ifndef CALL_ONCE
#define CALL_ONCE

#include <QtGlobal>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>
#include <QThreadStorage>
#include <QThread>


namespace CallOnce {
    /**
     * @brief 单次调用状态枚举
     * CO_Request: 请求执行
     * CO_InProgress: 正在执行
     * CO_Finished: 已完成
     */
    enum ECallOnce {
        CO_Request,      //!< 请求执行
        CO_InProgress,   //!< 正在执行
        CO_Finished      //!< 已完成
    };

    /**
     * @brief 线程局部存储的 once_flag，每个线程独立
     */
    Q_GLOBAL_STATIC(QThreadStorage<QAtomicInt*>, once_flag)
}


/**
 * @brief 全局线程安全单次调用，适用于单例/全局资源初始化
 * @tparam Function 可调用对象类型
 * @param func 需要只执行一次的函数/操作
 * @param flag 全局原子标志位，需外部静态保存
 * @note 多线程下保证func只被执行一次，其他线程阻塞等待
 * @note 兼容不同Qt版本的原子操作API
 */
template <class Function>
inline static void qCallOnce(Function func, QBasicAtomicInt& flag)
{
    using namespace CallOnce;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    int protectFlag = flag.fetchAndStoreAcquire(flag);
#elif QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    int protectFlag = flag.fetchAndStoreAcquire(flag.loadRelaxed());
#else
    int protectFlag = flag.fetchAndStoreAcquire(flag.load());
#endif

    if (protectFlag == CO_Finished)
        return;
    // 只有一个线程能将状态从CO_Request置为CO_InProgress并执行func
    if (protectFlag == CO_Request && flag.testAndSetRelaxed(protectFlag, CO_InProgress)) {
        func();
        flag.fetchAndStoreRelease(CO_Finished);
    }
    else {
        // 其他线程自旋等待，直到状态变为CO_Finished
        do {
            QThread::yieldCurrentThread();
        }
        while (!flag.testAndSetAcquire(CO_Finished, CO_Finished));
    }
}


/**
 * @brief 线程局部单次调用，每个线程只执行一次func
 * @tparam Function 可调用对象类型
 * @param func 需要每线程只执行一次的函数/操作
 * @note 适合线程私有资源的延迟初始化
 */
template <class Function>
inline static void qCallOncePerThread(Function func)
{
    using namespace CallOnce;
    if (!once_flag()->hasLocalData()) {
        once_flag()->setLocalData(new QAtomicInt(CO_Request));
        qCallOnce(func, *once_flag()->localData());
    }
}

#endif // CALL_ONCE


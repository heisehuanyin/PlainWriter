# 1.0版文本小说写作助手设计思路
## 工作过程
1. 设计全书剧情总概述
2. 设计分卷剧情梗概
3. 设计关键剧情梗概
4. 设计关键剧情分解点（提示用）
5. 在分卷节点下建立章节节点
6. 写章节细纲，爽点和结构安排，伏笔合并和埋设
7. 写正文并修正相关细纲，伏笔合并和埋设


## 界面设计
* 主界面
    * 大纲编辑界面
        * 大纲结构树
        * 全书大纲编辑^1
        * 分卷所有纲目编辑^2
        * 伏笔显示
            * 卷宗下所有伏笔（名称、吸附状态、前描述、后描述、吸附章节、源剧情）^3
            * 至此卷宗未闭合伏笔（名称、闭合状态、前描述、后描述、闭合章节、剧情名、卷宗名）^4.1
    * 正文编辑界面
        * 全文检索界面<1---->
        * 全书大纲编辑^1
        * 卷宗所有纲目大纲^2
        * 卷章节点编辑界面<1---->
        * 章节细纲编辑<2>
        * 正文编辑区域<1>
        * 伏笔显示
            * 卷宗下所有伏笔（名称、吸附状态、前描述、后描述、吸附章节、源剧情）^3
            * 至此章节未闭合伏笔（名称、闭合状态、前描述、后描述、闭合章节、剧情名、卷宗名）^4.2
    * 名词管理界面

## 后台模型设计
* novel-host
    * tab0
        * treeModel：大纲结构显示
        * textDoc：全书大纲编辑^1
        * textDoc：卷宗各种纲目编辑^2
        * tableModel：卷宗下所有伏笔^3
        * tableModel：至此卷宗未闭合伏笔^4
    * tab1
        * TableModel：全书检索结果显示
        * textDoc：全书大纲显示^1
        * textDoc：分卷大纲显示及关键剧情细纲，及要点细纲^2
        * treeModel：章卷管理模型
        * textDoc：章节细纲编辑
        * textDoc：正文内容编辑
        * tableModel：卷宗下所有伏笔^3
        * tableModel：至此章节未必和伏笔^4.2

## 控制器设计
* 输入大纲节点，切换伏笔合并与埋设结果
    * 获取所有伏笔列表，标注伏笔状态（已吸附表示可用，悬空表示不可用），标注伏笔来源
    * 统计所有未闭合伏笔，标注伏笔来源
* 输入大纲节点，切换大纲细节
    * 全书大纲是固定指向，不切换
    * 卷宗指向发生变化，卷宗细纲整体切换
    * 修改大纲文档内容，实时同步到大纲树
    * 点击卷宗内节点，文档自动跳转到指定区域
    * 以卷宗为单位计算和追踪伏笔情况
* 输入章卷节点，切换正文显示内容
    * 修改正文显示内容，修改存储内容
* 输入章卷节点，切换节点概要内容
    * 修改显示内容编辑节点概要内容，自动向下同步
    * 输入卷内大纲节点，跳转大纲内容
* 检索全文，显示结果
    * 点击结果条目，跳转文档


1. 大纲界面：
    1. 指定关键剧情下新建悬空伏笔
    2. 展示本卷内所有伏笔【剧情节点，伏笔标题，状态（悬空，吸附）】
    3. 展示前文至今所有未闭合伏笔【剧情节点，伏笔标题】
    
2. 编辑界面
    1. 指定章节下【关键事件下】建立并吸附伏笔
    2. 吸附本卷内悬空伏笔【剧情节点：伏笔名称】
    3. 闭合至今未闭合伏笔【[卷宗名称]剧情节点：伏笔名称】
    4. 显示本卷内所有伏笔【剧情节点，伏笔标题，状态（悬空，吸附）】
    5. 显示前文至今所有未闭合伏笔【卷宗节点，剧情节点，伏笔标题】


大纲文档显示
setcurrentoutlinenode
编辑界面只能设置卷节点
    编辑界面要点树实时更新，编辑界面卷大纲及下细纲更新
        点击要点树，卷大纲界面跳转
点击章节名称，刷新章细纲

大纲界面能够设置任何节点，编辑节点树
    全书大纲，卷大纲及下细纲，更新卷大纲及细纲，更新节点内容
        点击大纲树节点，卷大纲界面跳转

# 2.0版文本小说写作助手现状
## 设定目标
1. 基于大纲层次下降管理，辅助写作
	2. 全书大纲
	3. 分卷大纲
	4. 章节大纲
5. 伏笔管理
	6. 指定故事节点下建立伏笔
	7. 指定章节吸附伏笔（伏笔开端）
	8. 指定章节闭合伏笔（伏笔终结）

## 视图划分
1. 项目文档结构树
2. 小说故事结构树
3. 搜索结果呈现
4. 正文编辑视图
5. 卷宗大纲编辑视图
6. 章节大纲编辑视图
7. 全书大纲编辑视图
8. 卷宗内建伏笔吸附情况汇总
9. 卷宗可见伏笔吸附情况汇总
10. 章节可见伏笔吸附情况汇总

# 2.1版本文本小说写作助手
## 设定目标
1. 继承 2.0 版本
2. 基于sqlite数据库

## 表格设定
##### 作品信息表格（novel_basic）
键名| 名称 | 类型 | 备注
---|---|----|----
id | ID标识 | integer | primary key autoincrement
title | 字段名称 | text | not-null
content| 字段内容|text|nullable
comment|备注|text|nullable

##### 信息树结构组织（keys_tree）
键名| 名称 | 类型 | 备注
---|---|----|----
id| ID标识|integer|primary key autoincrement
type|节点类型|integer|（novel:-1，volume：0，chapter：1，storyblock：2，keypoint：3，despline：4）
parent|父节点|integer|foreign key <id>
nindex|索引|integer|
title|节点名称|text|
desp|节点描述|text

##### 文本内容组织（contents_collect）
键名| 名称 | 类型 | 备注
---|---|----|----
id| id标识|integer|primary key autoincrement
chapter_ref| 定向章节关联|integer|foreign key <keys_tree：id>
content|文章内容|text

##### 故事线驻点(points_collect)
键名| 名称 | 类型 | 备注
---|---|----|----
id|id标识|integer|primary key autoincrement
despline_ref| 故事线引用| integer|foreign key <keys_tree：id> notnull
chapter_attached|章节吸附|integer|foreign key<keys_tree：id>
story_attached|故事吸附|integer|foreign key<keys_tree：id>
nindex|索引|integer
close|闭合指示|integer|default 0
title|节点名称|text
desp|节点描述|text
 
## 视图改进
### 故事线视图
名称|索引|描述|起始卷|吸附章|关联剧情
---|---|---|---|---|---|---
节点名称|节点索引|描述|卷宗名|章节名|剧情名
* 故事线名称显示：
	* DisplayRole + Icon：【DisplayRole】
	* NodeType：{despline：1，attachpoint：2}【UserRole+1】
	* ParentVolumeIndex：第一视图，筛选本卷故事线【UserRole+2】
* 驻点名称显示
	* DisplayRole + Icon：【DisplayRole】
	* NodeType：{despline：1，attachpoint：2}【UserRole+1】
	* AttachedVolumeIndex：第二视图，标识本卷吸附点【UserRole+2】
	* AttachedChapterID：第三视图，标识本章吸附点【UserRole+3】
* 索引
	* DisplayRole：【DisplayRole】
	* itemID：【UserRole+1】
	
### 视图分类
* 本卷起始支线
* 本卷可见支线【在本卷内有吸附驻点闭合支线+继承的未闭合支线】
* 章节可见支线【继承的未闭合支线+本卷内吸附驻点的支线】

## 操作改进
### 支线操作
* 新增支线和删除支线——只能删除无任何驻点悬空支线
	* 章卷管理界面新建支线
		* 输入支线名称和描述，建立支线
	* 故事结构界面新建支线
		* 输入支线名称和描述，建立支线
	* 支线管理界面新建悬空支线
		* 输入支线名称和描述，建立支线
* 新增驻点和删除驻点——只能删除无任何情节和章节吸附的悬空驻点
	* 支线管理界面附增驻点、插入驻点
		* 输入驻点名称和描述，建立悬空驻点
		* 调整驻点索引，在时间顺序上重排驻点
	* 故事结构界面新增驻点
		* 选择剧情结构，计算合适位置【若位置在章吸附点前，人工确认是否继续】
		* 输入驻点名称和描述，插入驻点
		* 根据指定剧情节点，填充关联剧情字段
* 驻点关联管理
	* 章卷管理界面关联和解除关联
		* 选择支线名称
			* 在悬空列表中选择驻点
	* 故事结构树界面关联和解除关联
		* 选择支线名称
			* 悬空列表中选择驻点

# 2.2版文本小说写作助手规划
## 设定目标
1. 继承2.1版本规划
2. 引入自定义条目管理
	3. 自定义条目管理表格格式
	4. 完善的条目管理手段
		5. 创建
		6. 查询
		7. 修改
		8. 删除

## 表格结构（范定义表格）
##### 结构定义表格结构(tables_define)
键名|名称|类型|备注
---|---|---|---
id|ID标识|integer|primary key autoincrement
type|类型|integer|（tableRoot：-1，Column：0）
parent|父节点|integer|
nindex|索引|integer|not null
name|字段名称|text|
vtype|字段类型|int|（integer：0，string：1，enum：2，foreign-key：3）
supply|补充值|text|外键约束字符串 或 枚举字面值

##### 条目数据表格
键名|名称|类型|备注
---|---|---|---
id|ID|integer|primary key autoincrement
name|条目名称|text|所有表格都有的必填项
……|……|……|余下重复自定义字段



## 代码结构
### 表格结构定义类型节点
#### DefineNode
* 表格名称 -1
* 字段索引
* 字段名称
* 字段类型

# 2.3版文本小说写作助手规划
## 设定目标
1. 继承以上目标
2. 自由组合视图

## 表格结构
##### 视图配置表格(view_config)
键名|名称|类型|备注
---|---|---|---
id|unique-id|integer|primary key autoincrement not null
type|节点类型|integer|modeIndicator：0，Splitter：1，Selector：2
parent|父节点|integer|父节点 外键
nindex|索引|integer|not null （0 -> ∞）
supply|名称|text|模式名称，splitter-pos，selector-title



-----------------------------------------------
# 3.0版文本小说写作助手规划
## 设定目标
1. 基于大纲层次下降管理，辅助写作
	2. 全书大纲
	3. 分卷大纲
	4. 章节大纲
5. 故事线（伏笔、铺垫）管理
	6. 多故事线管理
	7. 分剧情节点与故事线脱壳
	7. 分卷分阶段指定主线
	8. 故事线内含时间顺序定位

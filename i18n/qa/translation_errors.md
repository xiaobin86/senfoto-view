<!--
  功能：lidarview 应用汉化「确定错误」清单（高置信度，供人工复核）。
  作者：acelan
  新建时间：2026-10-13
  修改时间：2026-10-13
-->

# 汉化「确定错误」清单（高置信度）

共 **256** 条。完整明细见 `translation_errors.csv`。


## cell → 细胞（应为「单元/单元格」） — 126 条

- `Cell Grid Point Probe` → 细胞网点探测  _(Clients_ParaView-XMLs.ts)_
- `If this property is set to 1 the output preserves the topology of its ` → 如果将这个属性设定为 1,输出会保留其输入的地形,并添加一个侧面阵列,以标记哪个单元格在内部或外部。 如果 0 , 输出  _(ServerManager-XMLs.ts)_
- `Nonlinear faces are approximated with flat polygons. This parameter co` → 非线性面部与扁平多边形相近. 此参数控制了多少次以细分非线性表面细胞. 更高的分区产生更近的近似,但需要更多的内存和渲染  _(ServerManager-XMLs.ts)_
- `If on, the output polygonal dataset will have a cell data array that h` → 如果打开,输出多边形数据集将有一个单元格数据数组,该数组将持有每个输出单元格的原3D单元格的单元格索引。 这对选择细胞有  _(ServerManager-XMLs.ts)_
- `If true, the filter will merge coincidental points (only if the ghost ` → 如果为真,过滤器会合并巧合点(只有在幽灵细胞阵列不存在的情况下).  _(ServerManager-XMLs.ts)_
- `Show/hide interior lines between cells. Only valid for multi cell text` → 显示/隐藏细胞间的内线. 只对多单元格文本有效。  _(ServerManager-XMLs.ts)_
- `Stitches images that are separated by one voxel, effectively generatin` → Stitches的图像被一个voxel分隔,在输入图像之间的新界面上有效生成幽灵点. 设置 NumberOfGhosts  _(ServerManager-XMLs.ts)_
- `Generate global point and cell ids. Global ids are unique ids where th` → 生成全局点和细胞ID. 全球ids是独一无二的ids,即使点(或单元格)在多个块或排位中重复,同一点(或单元格)也会被分  _(ServerManager-XMLs.ts)_
- … 其余 118 条见 CSV

## tolerance → 容忍（应为「容差」） — 37 条

- `Merge points within a distance specified by the **Tolerance** property` → 在** 容忍** 属性指定的距离内合并点。  _(ServerManager-XMLs.ts)_
- `Set the tolerance for merging points if **Merge Points** is enabled.` → 如果启用了** Morge Points** ,则设置合并点的容忍度。  _(ServerManager-XMLs.ts)_
- `Tolerance Is Absolute` → 容忍是绝对的  _(ServerManager-XMLs.ts)_
- `This property determines whether to treat the **Tolerance** property a` → 此属性决定了在执行点合并时是将**容忍**属性视为绝对(比容忍点更接近合并)还是相对(比输入数据边框对角长度的一小部分更  _(ServerManager-XMLs.ts)_
- `Specify the tolerance on the intersection. The tolerance is expressed ` → 指定十字路口的容忍度。 容忍度表示为附着面的边框对角的一小部分。  _(ServerManager-XMLs.ts)_
- `Set the radius used to snap to the closest cell when **Snap To Cell Wi` → 设定在启用 ** Snap to cell With closest Point ** 时用于向最接近的单元格坐标的半径  _(ServerManager-XMLs.ts)_
- `Specify the relative tolerance that will cause early termination.` → 指定导致提前终止的相对容忍度。  _(ServerManager-XMLs.ts)_
- `Compute Tolerance` → 计算容忍度  _(ServerManager-XMLs.ts)_
- … 其余 29 条见 CSV

## Decimate 误译（应为「精简」） — 23 条

- `Decimate Pro` → 决定 Pro  _(Clients_ParaView-XMLs.ts)_
- `Octree Image To Point Set` → 十进制图像到点集  _(Clients_ParaView-XMLs.ts)_
- `Decimation strategy that uses the eulerian distance between 3 consecut` → 使用连续3个点之间的eulerian距离来计算其错误的十进制策略.  _(ServerManager-XMLs.ts)_
- `Decimation Angle Strategy` → 十进制角度战略  _(ServerManager-XMLs.ts)_
- `Decimation strategy that uses the angle between 3 consecutive points t` → 使用连续3个点之间的角度来计算其错误的十进制策略.  _(ServerManager-XMLs.ts)_
- `Decimation Custom Field Strategy` → 十进制自定义字段策略  _(ServerManager-XMLs.ts)_
- `Decimation strategy that uses the values stored in a custom PointData ` → 使用自定义 PointData 阵列中存储的值作为该阵列毁灭量的度量的十进制策略 。 此数组的名称必须传递到 Field  _(ServerManager-XMLs.ts)_
- `Decimation Strategy` → 十进制战略  _(ServerManager-XMLs.ts)_
- … 其余 15 条见 CSV

## keyframe → 密钥框（应为「关键帧」） — 16 条

- `<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR` → <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www  _(Qt_Components.ts)_
- `Create an orbit path for selected keyframe, starting with current came` → 从当前相机开始为选定的密钥框架创建轨道路径 。  _(Qt_Components.ts)_
- `Apply selected keyframe configuration to the current view camera` → 在当前视图相机中应用选中的密钥框架配置  _(Qt_Components.ts)_
- `Key Frame Animation Cue` → 密钥框架动画 Cue  _(ServerManager-XMLs.ts)_
- `Animation cue with keyframes.` → 动画提示键框。  _(ServerManager-XMLs.ts)_
- `Key Frames` → 密钥框架  _(ServerManager-XMLs.ts)_
- `Last Added Key Frame Index` → 上次添加的密钥框架索引  _(ServerManager-XMLs.ts)_
- `Animation cue which can be used for animating Camera using keyframes.` → 动画提示,可以使用键框用于动画相机.  _(ServerManager-XMLs.ts)_
- … 其余 8 条见 CSV

## Glyph 误译（应为「图标/字形」） — 11 条

- `Oriented Glyph Contour Representation` → 定向格莱弗轮廓代表  _(ServerManager-XMLs.ts)_
- `Glyph Parameters` → 格莱弗参数  _(ServerManager-XMLs.ts)_
- `Glyph Type` → 格莱弗类型  _(ServerManager-XMLs.ts)_
- `Use Glyph Table` → 使用格言表  _(ServerManager-XMLs.ts)_
- `When true, culling pass and level of details are enabled. The culling ` → 当属实时,会启用拼接的通过和详细程度. 弯曲的传球探测到frustum视图外的点,LOD则根据相机和实例之间的距离显示被  _(ServerManager-XMLs.ts)_
- `This property determines which type of glyph will be placed at the poi` → 此属性决定了输入数据集的点将放置哪类格莱弗.  _(ServerManager-XMLs.ts)_
- `Glyph Transform` → 格莱弗变形  _(ServerManager-XMLs.ts)_
- `Glyph Mode` → 格莱弗模式  _(ServerManager-XMLs.ts)_
- … 其余 3 条见 CSV

## 快捷键被当词翻译（如 Ctrl+P→缩写+P） — 9 条

- `<html><head/><body><p>Use <span style=" font-weight:600;">P</span> to ` → <html><head/><body><p>使用<span style=" font-weight:600;">P</s  _(Qt_ApplicationComponents.ts)_
- `cosh` → 缩写  _(Qt_Components.ts)_
- `Decline` → 缩写  _(Qt_Core.ts)_
- `Should mipmaps be used for textures. Mipmaps are espcially useful when` → 应使用缩写来表达纹理 。 在处理大细纹理时,或者在您可能从远处查看纹理图的情况下,Mipmaps非常有用。  _(ServerManager-XMLs.ts)_
- `The compression algorithm used to compress binary data (appended mode ` → 用于压缩二进制数据的压缩算法(仅应用模式). 无提供最快的写法,LZ4提供最快的压缩写法,ZLib提供平衡性能,LZMA  _(ServerManager-XMLs.ts)_
- `Delimeter` → 缩略语  _(ServerManager-XMLs.ts)_
- `Faster Approximation` → 更快的缩写  _(ServerManager-XMLs.ts)_
- `The compression algorithm used to compress binary data (appended mode ` → 用于压缩二进制数据的压缩算法(仅应用模式). 无提供最快的写法,LZ4提供最快的压缩写法,ZLib提供均衡的性能,LZM  _(ServerManager-XMLs.ts)_
- … 其余 1 条见 CSV

## Focal Point → 联络点（应为「焦点」） — 7 条

- `Focal Point` → 联络点  _(Qt_ApplicationComponents.ts)_
- `Focal Point` → 联络点  _(Qt_Components.ts)_
- `Focal Point Info` → 联络点信息  _(ServerManager-XMLs.ts)_
- `Focal Point` → 联络点  _(ServerManager-XMLs.ts)_
- `Camera Focal Point Info` → 相机联络点信息  _(ServerManager-XMLs.ts)_
- `Camera Focal Point` → 相机联络点  _(ServerManager-XMLs.ts)_
- `Focal Point Mode` → 联络点模式  _(ServerManager-XMLs.ts)_

## View Up/Angle、Up Direction 误译 — 7 条

- `View Angle` → 查看角度  _(Qt_Components.ts)_
- `View Up` → 向上查看  _(Qt_Components.ts)_
- `Camera View Up Info` → 相机向上查看信息  _(ServerManager-XMLs.ts)_
- `Camera View Up` → 相机向上查看  _(ServerManager-XMLs.ts)_
- `View Angle LOD Threshold` → 查看角度 LOD 阈值  _(ServerManager-XMLs.ts)_
- `View Up` → 向上查看  _(ServerManager-XMLs.ts)_
- `View Angle` → 查看角度  _(ServerManager-XMLs.ts)_

## swept surface → 覆盖表面（应为「扫掠面」） — 5 条

- `The Linear Extrusion filter creates a swept surface by translating the` → Linear Extriusion 过滤器通过按照指定的向量平移输入数据集来创建覆盖的表面. 此过滤器旨在操作于2D多边  _(ServerManager-XMLs.ts)_
- `This filter creates a swept surface defined by translating the input a` → 这个过滤器通过将输入沿向量平移来创建覆盖的表面定义.  _(ServerManager-XMLs.ts)_
- `The value of this property indicates whether to cap the ends of the sw` → 此属性的值表示是否将覆盖的表面的末端封顶 。 通过将输入数据集的副本放置在横扫的表面的任意一端,按键工作,所以如果输入是  _(ServerManager-XMLs.ts)_
- `This filter generates a swept surface while translating the input alon` → 此过滤器在沿圆形路径平移输入时生成一个覆盖的表面.  _(ServerManager-XMLs.ts)_
- `Generates a swept surface using a rotational path.` → 使用旋转路径生成覆盖的表面 。  _(ServerManager-XMLs.ts)_

## Spatio Temporal 误译 — 3 条

- `Spatio Temporal Harmonics` → 气温谐波  _(Clients_ParaView-XMLs.ts)_
- `Spatio Temporal Harmonics Source` → 气温谐波源  _(Clients_ParaView-XMLs.ts)_
- `Spatio Temporal Harmonics` → 气温谐波  _(ServerManager-XMLs.ts)_

## Extrusion 误译（应为「挤出」） — 2 条

- `This property specifies the input to the Linear Cell Extrusion filter.` → 此属性指定了对 Linear 单元格突变过滤器的输入 。  _(ServerManager-XMLs.ts)_
- `If the value of this property is set to 1, coincident vertices will be` → 如果此属性的值被设定为 1, 则在切变发生后, 重叠的顶点将被合并 。 合并期间只考虑几何,遇到的第一个顶点是将使用点属  _(ServerManager-XMLs.ts)_

## Position → 立场（应为「位置」） — 2 条

- `Position` → 立场  _(Qt_Components.ts)_
- `Position` → 立场  _(ServerManager-XMLs.ts)_

## legacy → 遗产（应为「旧版」） — 2 条

- `Legacy Particle Path` → 遗产粒子路径  _(ServerManager-XMLs.ts)_
- `Writer to write vtkPolyData in a legacy vtk data file. This version is` → 将 vtkPolyData 写入遗产 vtk 数据文件的 Writer. 此版本在平行运行时使用. 它收集到第一个节点的  _(ServerManager-XMLs.ts)_

## translate(几何) → 翻译（应为「平移」） — 2 条

- `Reads an spcth history file where each row translates into a single ti` → 读取一个 spcth 历史文件,其中每行翻译为单个时间步,列为点,材料和属性.  _(ServerManager-XMLs.ts)_
- `The Warp By Scalar filter translates the points of the input dataset a` → Worp By Scalar 过滤器将输入数据集的点沿矢量翻译为由指定的scalars决定的距离. 这种滤波器在多边形、  _(ServerManager-XMLs.ts)_

## Crop 误译（应为「裁剪」） — 1 条

- `Cropping Origin` → 作物来源  _(ServerManager-XMLs.ts)_

## Favorites 误译 — 1 条

- `This filter uses a greedy algorithm to convert triangles into triangle` → 此过滤器使用一个贪婪的算法将三角形转换成三角形条形  _(ServerManager-XMLs.ts)_

## Volume Of Revolution 误译 — 1 条

- `The direction of the axis of revolution.` → 革命轴线的方向.  _(ServerManager-XMLs.ts)_

## solid → 固件（应为「实体」） — 1 条

- `If this property is set to 1, the open ends of the swept surface will ` → 如果将此属性设定为 1,则将覆盖表面的开口端封顶,并带有输入数据集的副本. 如果输入是一个由填充的多边形组成的2D表面,  _(ServerManager-XMLs.ts)_
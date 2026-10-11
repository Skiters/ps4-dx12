"""Export exact Git objects, verify dependencies, add bounded hooks; never runs a game."""
import argparse,io,json,pathlib,re,shutil,subprocess,tarfile,hashlib,configparser
REV='c7e065d1b415be16c23e260a21f1dd8bbfc4cb57'
def git(repo,*args):return subprocess.check_output(['git',*args],cwd=repo)
def attach_metadata(repo,rev,out):
    subprocess.run(['git','init','-q','--initial-branch=reference-profile',str(out)],check=True)
    obj=git(repo,'rev-parse','--git-path','objects').decode().strip()
    p=out/'.git/objects/info/alternates';p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(str((repo/obj).resolve()).replace('\\','/')+'\n',encoding='utf-8')
    (out/'.git/HEAD').write_text(rev+'\n',encoding='utf-8')
def export(repo,rev,out):
    out.mkdir(parents=True,exist_ok=True)
    data=git(repo,'archive','--format=tar',rev)
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        for m in tar.getmembers():
            if m.isfile():
                p=out/m.name
                if not p.resolve().is_relative_to(out.resolve()):raise RuntimeError('Unsafe archive member')
                p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(tar.extractfile(m).read())
    modules=configparser.ConfigParser()
    module_data=subprocess.run(['git','show',rev+':.gitmodules'],cwd=repo,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
    if module_data.returncode==0:modules.read_string(module_data.stdout.decode())
    urls={modules[s]['path']:modules[s]['url'] for s in modules.sections()}
    for line in git(repo,'ls-tree','-r',rev).decode().splitlines():
        left,name=line.split('\t',1);mode,kind,sha=left.split()
        if mode=='160000':
            local=repo/name
            available=local.exists() and subprocess.run(['git','cat-file','-e',sha+'^{commit}'],cwd=local,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0
            if not available:
                local=out.parent/'.reference-dependency-objects'/sha
                local.mkdir(parents=True,exist_ok=True)
                subprocess.run(['git','init','--bare',str(local)],check=True,stdout=subprocess.DEVNULL)
                print('Fetching exact dependency:',name,sha,flush=True)
                subprocess.run(['git','fetch','--no-tags','--depth=1',urls[name],sha],cwd=local,check=True)
            export(local,sha,out/name)
    attach_metadata(repo,rev,out)
def scope(text,symbol,label,args=''):
    matches=list(re.finditer(re.escape(symbol)+r'\s*\(',text))
    definitions=[]
    for m in matches:
        pos=m.end();depth=1
        while depth and pos<len(text):
            if text[pos]=='(':depth+=1
            elif text[pos]==')':depth-=1
            pos+=1
        tail=text[pos:pos+250];brace=tail.find('{');semi=tail.find(';')
        if brace>=0 and (semi<0 or brace<semi):definitions.append(pos+brace+1)
    if not definitions:return text,0
    for at in reversed(definitions):text=text[:at]+'\n    ReferenceTrace::Scope reference_scope("'+label+'"'+args+');'+text[at:]
    return text,len(definitions)

def queue_timestamps(out):
    """Hook actual recorded sessions and submission ticks; never infer frame ownership."""
    p=out/'src/video_core/renderer_vulkan/vk_scheduler.h';text=p.read_text(encoding='utf-8')
    text=text.replace('namespace Vulkan {','namespace ReferenceTrace { class GpuScope; }\nnamespace Vulkan {',1)
    text=text.replace('vk::CommandBuffer primary{};','vk::CommandBuffer primary{};\n        uint64_t reference_id{};',1)
    text=text.replace('bool is_rendering = false;','''bool is_rendering = false;
    std::unique_ptr<ReferenceTrace::GpuScope> reference_primary, reference_upload;''',1)
    p.write_text(text,encoding='utf-8')
    p=out/'src/video_core/renderer_vulkan/vk_scheduler.cpp';text=p.read_text(encoding='utf-8')
    text='#include "common/reference_gpu.h"\n'+text
    text=text.replace('void Scheduler::BeginSession() {','void Scheduler::BeginSession() {\n    ReferenceTrace::Scope reference_record("queue.record", reinterpret_cast<uint64_t>(this), CurrentTick());',1)
    text=text.replace('Check(session.primary.begin(begin_info));','''Check(session.primary.begin(begin_info));
    session.reference_id = reference_record.id;
    reference_primary = std::make_unique<ReferenceTrace::GpuScope>(instance, *this, session.primary, "gpu.queue.primary");''',1)
    text=text.replace('Check(upload_cmdbuf.begin(begin_info));','''Check(upload_cmdbuf.begin(begin_info));
    reference_upload = std::make_unique<ReferenceTrace::GpuScope>(instance, *this, upload_cmdbuf, "gpu.queue.upload");''',1)
    text=text.replace('Check(session.upload.end());','reference_upload.reset();\n        Check(session.upload.end());',1)
    text=text.replace('EndRendering();\n    Check(session.primary.end());','EndRendering();\n    reference_primary.reset();\n    Check(session.primary.end());',1)
    text=text.replace('cmd_buffers.push_back(session.primary);','''cmd_buffers.push_back(session.primary);
        ReferenceTrace::Emit("queue.record_submit", session.reference_id, reinterpret_cast<uint64_t>(this), signal_value);''',1)
    text=text.replace('auto submit_result = instance.GetGraphicsQueue().submit(submit_info, info.fence);','''ReferenceTrace::Emit("queue.submit", reinterpret_cast<uint64_t>(this), signal_value, info.num_wait_semas, cmd_buffers.size());
    auto submit_result = instance.GetGraphicsQueue().submit(submit_info, info.fence);''',1)
    p.write_text(text,encoding='utf-8')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--repository',type=pathlib.Path,required=True);ap.add_argument('--output',type=pathlib.Path,required=True);ap.add_argument('--resume-export',action='store_true');ap.add_argument('--instrument-only',action='store_true');a=ap.parse_args()
    # Validate the source object before touching an existing export/build.
    git(a.repository,'cat-file','-e',REV+'^{commit}')
    if a.output.exists() and not (a.resume_export or a.instrument_only):raise RuntimeError('Output already exists; never overwrite a previous reference build')
    if not a.instrument_only:export(a.repository,REV,a.output)
    here=pathlib.Path(__file__).parent
    for name in ['reference_trace.h','reference_gpu.h','reference_frames.h']:shutil.copyfile(here/name,a.output/'src/common'/name)
    plan={
      'src/video_core/buffer_cache/buffer_cache.cpp':{
        'BufferCache::ObtainBuffer':', device_addr, size, is_written, is_texel_buffer',
        'BufferCache::EnsureResident':', first_block, last_block',
        'BufferCache::SynchronizeMemory':', device_addr, size, is_written, is_texel_buffer',
        'BufferCache::DownloadMemory':', device_addr, size',
        'BufferCache::ReadMemory':', device_addr, size, is_write',
        'BufferCache::InvalidateMemory':', device_addr, size',
        'BufferCache::GetArena':', first_block, last_block',
        'BufferCache::ObtainBufferForImage':', device_addr, size',
        'BufferCache::SubmitPendingArenaBinds':''},
      'src/video_core/renderer_vulkan/vk_rasterizer.cpp':{
        'Rasterizer::Draw':', is_indexed, index_offset','Rasterizer::DrawIndirect':', is_indexed, arg_address, max_count',
        'Rasterizer::DispatchDirect':'','Rasterizer::DispatchIndirect':', address, offset, size',
        'Rasterizer::BindIndexBuffer':', index_offset','Rasterizer::BindResources':'',
        'Rasterizer::PrepareRenderState':'','Rasterizer::BeginRendering':'',
        'Rasterizer::CopyBuffer':', dst, src, num_bytes','Rasterizer::FillBuffer':'',
        'Rasterizer::ReadMemory':', addr, size','Rasterizer::InvalidateMemory':', addr, size'},
      'src/video_core/renderer_vulkan/vk_scheduler.cpp':{
        'Scheduler::Flush':'','Scheduler::Finish':'','Scheduler::Wait':', tick',
        'Scheduler::SubmitExecution':'','Scheduler::BeginRendering':'','Scheduler::EndRendering':'',
        'Scheduler::PopPendingOperations':''},
      'src/video_core/renderer_vulkan/vk_runtime.cpp':{
        'Runtime::CopyBuffer':'','Runtime::FillBuffer':'','Runtime::UploadImage':'','Runtime::DownloadImage':'',
        'Runtime::CopyImage':'','Runtime::AccessBuffer':', offset, size','Runtime::FlushBarriers':''},
      'src/video_core/texture_cache/texture_cache.cpp':{
        'TextureCache::RefreshImage':'','TextureCache::FindImage':'','TextureCache::FindTexture':'',
        'TextureCache::DownloadImageMemory':', image_id.index, sync','TextureCache::InvalidateMemory':', addr, size',
        'TextureCache::InvalidateMemoryFromGPU':', address, max_size','TextureCache::TickFrame':''},
      'src/video_core/texture_cache/tile_manager.cpp':{'TileManager::DetileImage':', info.guest_address, info.guest_size, static_cast<unsigned>(info.tile_mode), info.num_bits','TileManager::TileImage':''},
      'src/video_core/renderer_vulkan/vk_pipeline_cache.cpp':{'PipelineCache::GetGraphicsPipeline':'','PipelineCache::GetComputePipeline':'','PipelineCache::CreateGraphicsPipeline':'','PipelineCache::CreateComputePipeline':''},
      'src/video_core/renderer_vulkan/vk_pipeline_serialization.cpp':{'PipelineCache::WarmUp':''},
      'src/video_core/renderer_vulkan/vk_presenter.cpp':{'Presenter::PrepareFrame':'','Presenter::GetRenderFrame':''},
      'src/video_core/renderer_vulkan/vk_swapchain.cpp':{'Swapchain::Present':''},
      'src/core/libraries/videoout/video_out.cpp':{'sceVideoOutSubmitFlip':', bufferIndex, flipArg'},
      'src/core/libraries/kernel/threads/mutex.cpp':{'PthreadMutex::Lock':', reinterpret_cast<uintptr_t>(this), usec','PthreadMutex::Unlock':', reinterpret_cast<uintptr_t>(this)'},
      'src/core/libraries/kernel/threads/condvar.cpp':{'PthreadCond::Wait':', reinterpret_cast<uintptr_t>(this), usec','PthreadCond::Signal':', reinterpret_cast<uintptr_t>(this)'},
      'src/core/libraries/kernel/threads/semaphore.cpp':{'sceKernelWaitSema':'','sceKernelSignalSema':''},
      'src/core/libraries/kernel/threads/event_flag.cpp':{'sceKernelWaitEventFlag':''},
      'src/core/memory.cpp':{'MemoryManager::MapMemory':'','MemoryManager::UnmapMemory':'','MemoryManager::Protect':', addr, size'},
      'src/core/libraries/videoout/driver.cpp':{'VideoOutDriver::SubmitFlip':', index, flip_arg, is_eop','VideoOutDriver::SubmitFlipInternal':', index, flip_arg, is_eop'},
      'src/video_core/page_manager.cpp':{'PageManager::OnGpuMap':', address, size','PageManager::OnGpuUnmap':', address, size','PageManager::UpdatePageWatchers':', addr, size, static_cast<unsigned>(write_op)'},
    }
    hooks=[]
    # Arguments referring to version-dependent structured types are intentionally omitted.
    plan['src/video_core/texture_cache/texture_cache.cpp']['TextureCache::DownloadImageMemory']=''
    plan['src/video_core/renderer_vulkan/vk_pipeline_cache.cpp']['PipelineCache::CompileModule']=', info.pgm_hash, perm_idx'
    plan['src/video_core/buffer_cache/buffer.cpp']={'StreamBuffer::PrepareMap':', size, alignment, allow_wait','StreamBuffer::WaitPendingOperations':'','StreamBuffer::Commit':''}
    plan['src/core/libraries/kernel/time.cpp']={'sceKernelUsleep':', microseconds','sceKernelNanosleep':''}
    plan['src/video_core/texture_cache/image.cpp']={'UniqueImage::Create':'','UniqueImage::Destroy':'','Image::GetBarriers':''}
    plan['src/video_core/texture_cache/texture_cache.cpp'].update({'TextureCache::RegisterImage':'','TextureCache::UnregisterImage':'','TextureCache::ProcessDownloadImages':''})
    plan['src/video_core/buffer_cache/buffer_cache.cpp'].update({'BufferCache::SynchronizeMemoryFromImage':', device_addr, size','BufferCache::SynchronizeDmaBuffers':''})
    plan['src/video_core/buffer_cache/memory_tracker.h']={name:', cpu_addr, size' for name in ['MarkRegionAsCpuModified','UnmarkRegionAsGpuModified','InvalidateRegion','ForEachUploadRange','ForEachDownloadRange']}
    plan['src/video_core/renderer_vulkan/vk_pipeline_cache.cpp']['PipelineCache::GetProgram']=''
    plan['src/video_core/renderer_vulkan/vk_staging_buffer_pool.cpp']={'StagingBufferPool::Request':', size, static_cast<unsigned>(type), alignment, deferred','StagingBufferPool::RequestLarge':', size, static_cast<unsigned>(type), deferred, unsynchronized','StagingBufferPool::RequestFromRing':', size, alignment, static_cast<unsigned>(type), unsynchronized','StagingBufferPool::FreeDeferred':'','StagingBufferPool::TickFrame':''}
    plan['src/video_core/renderer_vulkan/vk_semaphore.cpp']={'Semaphore::Wait':', tick, reinterpret_cast<uintptr_t>(this)','Semaphore::Refresh':''}
    if a.instrument_only:
        for rel in set(plan)|{'src/video_core/amdgpu/liverpool.cpp','src/video_core/renderer_vulkan/vk_presenter.h','src/video_core/renderer_vulkan/vk_scheduler.h'}:
            p=a.output/rel
            if p.exists():p.write_bytes(git(a.repository,'show',REV+':'+rel))
    for rel,functions in plan.items():
        p=a.output/rel
        if not p.exists():hooks.append({'file':rel,'status':'missing_file'});continue
        text=p.read_text(encoding='utf-8');found=0
        if rel.endswith('vk_semaphore.cpp'):
            # The Win32 thread CPU clock header defines WAIT_TIMEOUT; avoid a macro collision.
            text=text.replace('WAIT_TIMEOUT','REFERENCE_WAIT_TIMEOUT')
        for symbol,args in functions.items():
            text,n=scope(text,symbol,rel.split('/')[-1]+':'+symbol,args);found+=n
            hooks.append({'file':rel,'symbol':symbol,'definitions':n,'status':'hooked' if n else 'requires_followup'})
        if found:p.write_text('#include "common/reference_trace.h"\n'+text,encoding='utf-8')
    # Packet counts only: coroutine lifetime scopes would give invalid timing across yields.
    p=a.output/'src/video_core/amdgpu/liverpool.cpp';text=p.read_text(encoding='utf-8')
    packet_site=iter(range(3))
    text,n=re.subn(r'(const (?:auto|PM4ItOpcode) opcode = header->type3\.opcode;)',lambda m:m.group(1)+'\n        ReferenceTrace::Packet(static_cast<unsigned>(opcode), '+str(next(packet_site))+');',text)
    if not n:
        text,n=re.subn(r'(switch \(header->type3\.opcode\) \{)',r'ReferenceTrace::Emit("pm4.opcode", static_cast<unsigned>(header->type3.opcode));\n        \1',text)
    p.write_text('#include "common/reference_trace.h"\n'+text,encoding='utf-8');hooks.append({'file':str(p.relative_to(a.output)),'symbol':'PM4 packet opcode','definitions':n,'status':'hooked' if n else 'requires_followup'})
    # Exact byte volume at the actual transfer sites, separate from requested buffer spans.
    p=a.output/'src/video_core/buffer_cache/buffer_cache.cpp';text=p.read_text(encoding='utf-8')
    text=text.replace('if (bind_ranges.Empty()) {','if (bind_ranges.Empty()) {\n        ReferenceTrace::Emit("buffer.resident.hit", (last_block-first_block+1) << block_shift, first_block, last_block);')
    text=text.replace('if (!copies.empty()) {','ReferenceTrace::Emit("buffer.sync.result", total_size_bytes, size, copies.size());\n    if (!copies.empty()) {',1)
    text=text.replace('stream_buffer.Commit();','stream_buffer.Commit();\n        ReferenceTrace::Emit("buffer.stream.cpu_copy.bytes", size, device_addr);',1)
    text=text.replace('info.AddWait(signal_sema, signal_tick);','ReferenceTrace::Emit("buffer.sparse.bind", buffer_binds.size(), signal_tick);\n    info.AddWait(signal_sema, signal_tick);')
    text=text.replace('runtime.CopyBuffer(staging.buffer, arena, copies);','ReferenceTrace::Emit("buffer.upload.bytes", total_size_bytes, device_addr, size);\n        runtime.CopyBuffer(staging.buffer, arena, copies);')
    text=text.replace('runtime.CopyBuffer(arena, download.buffer, copies);','ReferenceTrace::Emit("buffer.download.bytes", total_size_bytes, device_addr, size);\n    runtime.CopyBuffer(arena, download.buffer, copies);')
    text=text.replace('const auto device_memory = Vulkan::Check(instance.GetDevice().allocateMemory(alloc_info));','ReferenceTrace::Emit("buffer.physical.allocate", alloc_info.allocationSize, resident_blocks);\n    const auto device_memory = Vulkan::Check(instance.GetDevice().allocateMemory(alloc_info));')
    p.write_text(text,encoding='utf-8')
    p=a.output/'src/video_core/texture_cache/image.cpp';text=p.read_text(encoding='utf-8')
    text=text.replace('size_bytes = alloc_info.size;','size_bytes = alloc_info.size;\n    ReferenceTrace::Emit("image.physical.allocate", alloc_info.size, alloc_info.memoryType, image_ci.extent.width, image_ci.extent.height);')
    text=text.replace('vmaDestroyImage(allocator, image, allocation);','ReferenceTrace::Emit("image.physical.free", size_bytes);\n        vmaDestroyImage(allocator, image, allocation);')
    p.write_text(text,encoding='utf-8')
    p=a.output/'src/core/libraries/videoout/driver.cpp';text=p.read_text(encoding='utf-8')
    text=text.replace('frame = presenter->PrepareFrame(group, buffer.address_left);','frame = presenter->PrepareFrame(group, buffer.address_left);\n        ReferenceTrace::Emit("frame.guest_flip_link", index, flip_arg, is_eop, 0, frame->reference_frame_id);')
    p.write_text(text,encoding='utf-8')
    p=a.output/'src/video_core/renderer_vulkan/vk_pipeline_cache.cpp';text=p.read_text(encoding='utf-8')
    text=text.replace('const auto ir_program = Shader::TranslateProgram(code, pools, info, runtime_info, profile);','const auto ir_program = [&] { ReferenceTrace::Scope phase("shader.GCN_translate", info.pgm_hash); return Shader::TranslateProgram(code, pools, info, runtime_info, profile); }();')
    text=text.replace('auto spv = Shader::Backend::SPIRV::EmitSPIRV(profile, runtime_info, ir_program, binding);','auto spv = [&] { ReferenceTrace::Scope phase("shader.SPIRV_emit", info.pgm_hash); return Shader::Backend::SPIRV::EmitSPIRV(profile, runtime_info, ir_program, binding); }();')
    text=text.replace('auto [it_pgm, new_program] = program_cache.try_emplace(params.hash);','auto [it_pgm, new_program] = program_cache.try_emplace(params.hash);\n    ReferenceTrace::Emit("shader.program.lookup", params.hash, new_program);')
    for kind,key,typ in [('graphics','graphics_key','GraphicsPipelineKey'),('compute','compute_key','ComputePipelineKey')]:
        needle='const auto [it, is_new] = '+kind+'_pipelines.try_emplace('+key+');'
        text=text.replace(needle,needle+'\n    ReferenceTrace::Emit("pipeline.'+kind+'.lookup", std::hash<'+typ+'>{}('+key+'), is_new);')
    p.write_text(text,encoding='utf-8')
    queue_timestamps(a.output)
    # GPU timestamps surround actual dispatch recording, and are read after an existing timeline completion.
    for rel,needle,label in [('src/video_core/texture_cache/tile_manager.cpp','cmdbuf.dispatch(dim_x, 1, 1);','gpu.detile_or_retile'),('src/video_core/renderer_vulkan/vk_rasterizer.cpp','cmdbuf.dispatch(cs_program.dim_x, cs_program.dim_y, cs_program.dim_z);','gpu.guest_compute'),('src/video_core/renderer_vulkan/vk_rasterizer.cpp','cmdbuf.dispatchIndirect(buffer->Handle(), base);','gpu.guest_compute_indirect')]:
        p=a.output/rel;text=p.read_text(encoding='utf-8');count=text.count(needle)
        text=text.replace(needle,'{ ReferenceTrace::GpuScope reference_gpu(instance, scheduler, cmdbuf, "'+label+'"); '+needle+' }')
        if 'common/reference_gpu.h' not in text:text='#include "common/reference_gpu.h"\n'+text
        p.write_text(text,encoding='utf-8');hooks.append({'file':rel,'symbol':label,'definitions':count,'status':'hooked' if count else 'requires_followup'})
    p=a.output/'src/video_core/renderer_vulkan/vk_rasterizer.cpp';text=p.read_text(encoding='utf-8')
    text=text.replace('const auto& cs = pipeline->GetStage(Shader::SwStage::Compute);','const auto& cs = pipeline->GetStage(Shader::SwStage::Compute);\n    ReferenceTrace::Emit("compute.shader", cs.pgm_hash, cs_program.dim_x, cs_program.dim_y, cs_program.dim_z);')
    text=text.replace('buffer_cache.ObtainBuffer(index_address, index_buffer_size, false);','buffer_cache.ObtainBuffer(index_address, index_buffer_size, false);\n    ReferenceTrace::Emit("index.binding", index_address, index_buffer_size, index_type == vk::IndexType::eUint16, regs.enable_primitive_restart);')
    p.write_text(text,encoding='utf-8')
    # Real prepared-frame identity follows the Frame object across reuse/presentation.
    p=a.output/'src/video_core/renderer_vulkan/vk_presenter.h';text=p.read_text(encoding='utf-8').replace('struct Frame {','struct Frame {\n    uint64_t reference_frame_id{};');p.write_text(text,encoding='utf-8')
    p=a.output/'src/video_core/renderer_vulkan/vk_presenter.cpp';text=p.read_text(encoding='utf-8')
    start=text.index('Frame* Presenter::PrepareFrame(');end=text.index('\n}',start)
    part=text[start:end];needle='return frame;'
    replacement='''ReferenceTrace::Emit("frame.return", frame->ready_tick, 0, 0, 0, frame->reference_frame_id);
    return frame;'''
    part=part.replace('Frame* frame = GetRenderFrame();','Frame* frame = GetRenderFrame();\n    frame->reference_frame_id = ReferenceTrace::NewFrame();')
    part=part.replace('draw_scheduler.Flush(info);','''ReferenceTrace::Emit("frame.ready", frame->ready_tick, frame->width, frame->height, reinterpret_cast<uint64_t>(&draw_scheduler), frame->reference_frame_id);
    draw_scheduler.DeferOperation([id = frame->reference_frame_id, tick = frame->ready_tick] {
        ReferenceTrace::Emit("frame.gpu_completion_observed", tick, 0, 0, 0, id);
        Common::GuestFrameTrace::Record(id, "gpu-complete", tick);
    });
    draw_scheduler.Flush(info);
    Common::GuestFrameTrace::Record(frame->reference_frame_id, "submitted", frame->ready_tick);''')
    if part.count(needle)!=1:raise RuntimeError('PrepareFrame identity anchor changed')
    text=text[:start]+part.replace(needle,replacement)+text[end:]
    text,n=scope(text,'Presenter::Present','Presenter::Present.frame_identity',', frame->reference_frame_id, is_reusing_frame, is_game_frame')
    p.write_text(text,encoding='utf-8')
    manifest={'reference_revision':REV,'mode':'B','hooks':hooks,'not_launched':True,
      'limits':['CPU scopes are nested wall/thread CPU measurements, not stack samples. Thread CPU clock is coarse for short calls.','Frame hints are not causal attribution of earlier PM4 work.','Queue-session GPU queries include primary/upload command buffers; only selected dispatches have operation-level queries. Graphics/copy exclusive operation timing remains pending.','No calibrated GPU/CPU clock alignment; duration-only GPU timings.','Guest runs through native x86 entry; no guest JIT cache metric is assumed.','Short compute.shader detail is filtered; do not infer retained shader identity from its aggregate count. Indirect shader identity remains pending.','Frame completion is observed at existing scheduler retirement; timestamp may lag GPU completion. Submitted CSV event occurs after Flush returns and can follow its already-completed retirement.','Detailed events shorter than 100 us are filtered by default, except frame/gpu/queue categories; aggregate counters still include every enabled call.','Independent frame CSV is bounded; dropped records and storage-cap status invalidate complete coverage. Marker checks use elapsed steady-clock time.'],
      'trace_header_sha256':hashlib.sha256((here/'reference_trace.h').read_bytes()).hexdigest()}
    (a.output/'reference-instrumentation.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'source':str(a.output),'hooks':sum(h.get('definitions',0) for h in hooks)},indent=2))
if __name__=='__main__':main()

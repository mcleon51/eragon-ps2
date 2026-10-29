// Cria funcoes em enderecos que a analise automatica do Ghidra nao descobriu.
// Util para alvos alcancados so por J direto ou por tabela de ponteiros
// (construtores estaticos, vtables, jump tables), que o fluxo normal perde.
//
// Uso headless: passar a lista via CreateFunctionsAt.properties
//   Create functions Addresses=0x151a80,0x151aa0
//
// @category PS2Recomp

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CreateFunctionsAt extends GhidraScript {

    @Override
    public void run() throws Exception {
        String raw = askString("Create functions", "Addresses");
        if (raw == null || raw.trim().isEmpty()) {
            println("CreateFunctionsAt: nenhuma entrada, nada a fazer");
            return;
        }

        int criadas = 0;
        int jaExistiam = 0;
        int falhas = 0;

        for (String parte : raw.split(",")) {
            String txt = parte.trim();
            if (txt.isEmpty()) {
                continue;
            }
            if (txt.startsWith("0x") || txt.startsWith("0X")) {
                txt = txt.substring(2);
            }

            Address addr;
            try {
                addr = currentProgram.getAddressFactory().getDefaultAddressSpace()
                        .getAddress(Long.parseLong(txt, 16));
            } catch (Exception e) {
                println("  ENDERECO INVALIDO: " + parte.trim());
                falhas++;
                continue;
            }

            Function existente = getFunctionAt(addr);
            if (existente != null) {
                println("  ja existe: " + addr + " -> " + existente.getName());
                jaExistiam++;
                continue;
            }

            // Sem bytes desmontados nao ha como criar funcao: desmonta primeiro.
            if (getInstructionAt(addr) == null) {
                println("  desmontando " + addr);
                if (!disassemble(addr)) {
                    println("  FALHA ao desmontar " + addr);
                    falhas++;
                    continue;
                }
            }

            Function nova = createFunction(addr, null);
            if (nova == null) {
                println("  FALHA ao criar funcao em " + addr);
                falhas++;
                continue;
            }

            println("  CRIADA: " + addr + " -> " + nova.getName()
                    + " (tamanho " + nova.getBody().getNumAddresses() + ")");
            criadas++;
        }

        println("CreateFunctionsAt: criadas=" + criadas
                + " jaExistiam=" + jaExistiam + " falhas=" + falhas);
    }
}
